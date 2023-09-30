#include "WinGraph.h"
#include "xlsx.h"
#include "Mouse.h"
#include "math.h"
#include "datetimeapi.h"
#include "fileapi.h"

#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "User32.lib")
#pragma comment (lib, "Opengl32.lib")
#pragma comment (lib, "Glu32.lib")

// Private declarations goes here

enum { MAX_SIGNAL_COUNT = 64 };

VOID InitGL(HGRAPH hGraph, int Width, int Height);
VOID SetGLView(int Width, int Height);
VOID CheckErr(VOID);

BOOL BuildMyFont(HGRAPH hGraph, char* FontName, int Fontsize);
void KillFont(GLvoid);
GLvoid glPrint(const char* fmt, ...);

BOOL FindGlobalMaxScale(HGRAPH hGraph, double& Xmin, double& Xmax, double& Ymin, double& Ymax);
VOID DrawWave(HGRAPH hGraph);
VOID DrawString(float x, float y, char* string);
VOID DrawGraphSquare(VOID);
VOID DrawGridLines(VOID);
VOID DrawCursor(float x, float y);


inline double TakeFiniteNumber(double x);
double FindFirstFiniteNumber(double* tab, int length);
LPSTR dtos(LPSTR str, int len, double value);
double GetStandardizedData(double X, double min, double max);
VOID normalize_data(HGRAPH hGraph, double Xmin, double Xmax, double Ymin, double Ymax);
VOID UpdateBorder(HGRAPH hGraph);
INT GetBufferSize(HGRAPH hGraph);
BOOL GetUniqueFilename(CHAR* lpFilename, CHAR* lpFileExtension);


inline long long PerformanceFrequency();
inline long long PerformanceCounter();

typedef struct {
	double period_s;
	double min_value;
	double max_value;
	INT average_value_counter;
	double average_value_accumulator;
	double average_value;
}DATA_STATISTIC;

typedef struct {
	char signame[260];
	float color[3];
	bool show;
	DATA_STATISTIC stat;
	double*X;
	double*Y;
	double*Xnorm;
	double*Ynorm;
	double Xmin;
	double Xmax;
	double Ymin;
	double Ymax;
	double Yaverage;
}DATA;

VOID ZeroObject(DATA* pDATA, INT iBufferSize);
#pragma warning(disable : 4200)					// Disable warning: DATA* signal[] -> Array size [0], See CreateGraph for signal allocation specifics
typedef struct {
	HWND hParentWnd;							// Parent handle of the object
	HWND hGraphWnd;								// Graph handle
	HDC hDC;									// OpenGL device context
	HGLRC hRC;									// OpenGL rendering context
	INT totalsignalcount;						// Total signals in the struct
	INT signalcount;							// signals in use in the struct
	INT cur_nbpoints;							// Current total points in the arrays
	INT BufferSize;								// The total amount of point to handle							
	BOOL bRunning;								// Status of the graph
	LOGGER_M Logging;							// Logging type
	FILTER_M Filtering;							// Filtering type
	BOOL bAutoscale;							// Autoscale active
	BOOL bDisplayCursor;						// Logging active
	double ymin_fix;							// Fix the Y min val
	double ymax_fix;							// Fix the Y max val
	int scale_factor;						// Fix the X scale factor (zoom)
	double xwindow_fix;							// Fix the time windows val
	DATA* signal[];								// ! (flexible array member) Array of pointers for every signal to be store by the struct - Must be last member of the struct
}GRAPHSTRUCT, * PGRAPHSTRUCT;					// Declaration of the struct. To be cast from HGRAPH api

	// Global access

DATA* SnapPlot;									// SnapPlot: work with temp data on signals[], used to convert standard values to normalized values
RECT DispArea;									// RECT struct for the OpenGL area dimensions stored in WinProc
GLuint  base;                                   // Base Display List For The Font Set
SIZE dispStringWidth;							// The size  in pixel of "-0.000" displayed on screen
CRITICAL_SECTION cs;							// Sync purpose
FILE* logfile;									// The ascii log file
HEXCEL XL;										// The xlsx log file
INT runonce;									// Used by UpdateBorder

	// High precision time measurements

long long frequency;
long long start;
long long finish;

GLuint    PixelFormat;                          //Defining the pixel format to display OpenGL 
PIXELFORMATDESCRIPTOR pfd =
{
	sizeof(PIXELFORMATDESCRIPTOR),              // Size Of This Pixel Format Descriptor
	1,                                          // Version Number (?)
	PFD_DRAW_TO_WINDOW |                        // Format Must Support Window
	PFD_SUPPORT_OPENGL |						// Format Must Support OpenGL
	PFD_DOUBLEBUFFER,							// Must Support double Buffering
	PFD_TYPE_RGBA,								// Request An RGBA Format
	32,											// Select A 32Bit Color Depth
	0, 0, 0, 0, 0, 0,							// Color Bits Ignored (?)
	0,											// No Alpha Buffer
	0,											// Shift Bit Ignored (?)
	0,											// No Accumulation Buffer
	0, 0, 0, 0,									// Accumulation Bits Ignored (?)
	24,											// 32Bit Z-Buffer (Depth Buffer)
	8,											// No Stencil Buffer
	0,											// No Auxiliary Buffer (?)
	PFD_MAIN_PLANE,								// Main Drawing Layer
	0,											// Reserved (?)
	0, 0, 0										// Layer Masks Ignored (?)
};

/*-------------------------------------------------------------------------
	DllMain: DLL Entry point
  -------------------------------------------------------------------------*/

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call,
	LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return true;
}

/*-------------------------------------------------------------------------
	StartGraph: Setup a new log file and zero memory
  -------------------------------------------------------------------------*/

BOOL StartGraph(HGRAPH hGraph)
{
		// Sanity check

	if (NULL == hGraph)
	{
		printf("[!] Error at StartGraph() graph handle is null\n");
		return FALSE;
	}

	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	DATA* pDATA;

		// Sanity check

	if (TRUE == pgraph->bRunning)
	{
		printf("[!] Error at StartGraph() graph already running\n");
		return FALSE;
	}
		

	EnterCriticalSection(&cs);

		// reset counters and data array of signals

	pgraph->cur_nbpoints = 0;
	for (int index = 0; index < pgraph->signalcount; index++)
	{
		pDATA = pgraph->signal[index];
		memset(pDATA->X, 0, sizeof(double)* pgraph->BufferSize);
		memset(pDATA->Y, 0, sizeof(double) * pgraph->BufferSize);
		memset(pDATA->Xnorm, 0, sizeof(double) * pgraph->BufferSize);
		memset(pDATA->Ynorm, 0, sizeof(double) * pgraph->BufferSize);
		pDATA->Xmin = 0.0f;
		pDATA->Xmax = 0.0f;
		pDATA->Ymin = 0.0f;
		pDATA->Ymax = 0.0f;
		pDATA->show = true;
	}

		// Create the log file

	if (pgraph->Logging == LOGGER_ASCII)
	{
		logfile = NULL;

			// create unique filename

		char lpDateStr[MAX_PATH] = "";
		if (!GetUniqueFilename(lpDateStr, (char*)".lab"))
		{
			MessageBox(GetFocus(), "Error: impossible to generate an unique filename in the current directory", "Error", MB_ICONERROR);
			LeaveCriticalSection(&cs);
			return FALSE;
		}

			// try to open the file

		fopen_s(&logfile, lpDateStr, "w+");
		if (!logfile)
		{
			MessageBox(GetFocus(), "Error: impossible to read/write the file", "Error", MB_ICONERROR);
			LeaveCriticalSection(&cs);
			return FALSE;	
		}

			// make logfile header

		fprintf(logfile, "Time(s)");
		for (int u = 0; u < pgraph->signalcount; u++)
		{
			pDATA = (DATA*)pgraph->signal[u];
			fprintf_s(logfile, "\t%s", pDATA->signame);
		}
		fprintf_s(logfile, "\n");
	}

	if (pgraph->Logging == LOGGER_XLSX)
	{

		// create unique filename

		char lpDateStr[MAX_PATH] = "";
		if (!GetUniqueFilename(lpDateStr, (char*)".xlsx"))
		{
			MessageBox(GetFocus(), "Error: impossible to generate an unique filename in the current directory", "Error", MB_ICONERROR);
			LeaveCriticalSection(&cs);
			return FALSE;
		}

		// open Excel and load instance
		XL = excel_create_instance();

		// Write header
		char one_line[64] = "Logger header\tAnalog0\tAnalog1\t";
		//excel_addline(XL, one_line);
	}

		// Save the start time x=0

	frequency = PerformanceFrequency();
	start = PerformanceCounter();

		// Update status -> Graph ON

	pgraph->bRunning = TRUE;


	// reset runonce flag
	runonce = 0;
	LeaveCriticalSection(&cs);
	return TRUE;
}

/*-------------------------------------------------------------------------
	StopGraph: Close the logfile if needed and 
	update state flags
  -------------------------------------------------------------------------*/

VOID StopGraph(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at StopGraph() graph handle is null\n");
		return ;
	}

		//Close the log file properly

	EnterCriticalSection(&cs);
	if (pgraph->Logging == LOGGER_ASCII)
	{
		if (logfile)
		{
			fclose(logfile);
			logfile = NULL;
		}
	}
	if (pgraph->Logging == LOGGER_XLSX)
	{
		if (XL)
		{
			excel_drawgraph(XL);
			excel_save(XL, "test_excel.xlsx");
			excel_close(XL);
		}

	}

		// Update status -> Graph OFF

	pgraph->bRunning = FALSE;
	LeaveCriticalSection(&cs);
}

/*-------------------------------------------------------------------------
	FreeGraph: Free every buffer allocated by malloc
	Realease the device context and delete the object
  -------------------------------------------------------------------------*/

VOID FreeGraph(HGRAPH *hGraph)
{
		// Sanity check

	if (NULL == hGraph)
		return;

	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)*hGraph; // Take the reference not the value
	
	EnterCriticalSection(&cs);
	if (pgraph  !=NULL)
	{
		DATA* pDATA;
		for (int index = 0; index < pgraph->totalsignalcount; index++)
		{
			pDATA = pgraph->signal[index];
			if (pDATA)
			{
				if (pDATA->X)
					free(pDATA->X);

				if (pDATA->Y)
					free(pDATA->Y);

				if (pDATA->Xnorm)
					free(pDATA->Xnorm);

				if (pDATA->Ynorm)
					free(pDATA->Ynorm);

				free(pDATA);
			}			
		}

		wglMakeCurrent(pgraph->hDC, NULL);
		wglDeleteContext(pgraph->hRC);
		ReleaseDC(pgraph->hParentWnd, pgraph->hDC);
		free(pgraph);
		

		if (SnapPlot)
		{
			if (SnapPlot->X)
				free(SnapPlot->X);

			if (SnapPlot->Y)
				free(SnapPlot->Y);

			if (SnapPlot->Xnorm)
				free(SnapPlot->Xnorm);

			if (SnapPlot->Ynorm)
				free(SnapPlot->Ynorm);

			free(SnapPlot);
		}
			
		if (logfile)
		{
			fclose(logfile);
			logfile = NULL;
		}	
	}

	LeaveCriticalSection(&cs);
	DeleteCriticalSection(&cs);

	*hGraph = NULL;

}



/*-------------------------------------------------------------------------
	CreateGraph: Initialize the structure, signals,
	OpenGL and critical section. return HGRAPH
  -------------------------------------------------------------------------*/

HGRAPH CreateGraph(HWND hWnd, RECT GraphArea, INT SignalCount, INT BufferSize )
{
	int PFDID;
	static GRAPHSTRUCT* pgraph = NULL;

		// Sanity check

	if (NULL != pgraph) 
		return pgraph; 

	if (NULL == hWnd)
	{
		printf("[!] No control available to load the graph in CreateGraph()\n");
		return NULL;
	}

	if (0 == SignalCount || MAX_SIGNAL_COUNT < SignalCount || 0 >= BufferSize)
	{
		printf("[!] SignalCount not in range in CreateGraph()\n");
		return NULL;
	}

		// Initialyze sync

	InitializeCriticalSection(&cs);
	EnterCriticalSection(&cs);

		// Init struct

	if (NULL == (pgraph = (GRAPHSTRUCT*)malloc(sizeof(GRAPHSTRUCT) + sizeof(void*) * SignalCount)))		// Carefully taking in account signal declaration DATA*signal[], so allocate space for each new ptr on the fly
	{
		LeaveCriticalSection(&cs);
		printf("[!] malloc() failed in CreateGraph()\n");
		return NULL;	// Otherwize Heap will be corrupted
	}

		// Struct memory zero at startup

	memset(pgraph, 0, sizeof(GRAPHSTRUCT) + sizeof(void*) * SignalCount);

		// Allocate each signal buffers on the Heap and fill with zero

	for (int i = 0; i < SignalCount; i++)
	{
		if (NULL == (pgraph->signal[i] = (DATA*)malloc(sizeof(DATA))))
		{
			LeaveCriticalSection(&cs);
			printf("[!] malloc failed to build signals buffer in CreateGraph()\n");
			return NULL;
		}

		DATA* pDATA = pgraph->signal[i];

		if (NULL == (pDATA->X = (double*)malloc(sizeof(double) * BufferSize)))
		{
			LeaveCriticalSection(&cs);
			printf("[!] malloc failed to build signals buffer in CreateGraph()\n");
			return NULL;
		}

		if (NULL == (pDATA->Y = (double*)malloc(sizeof(double) * BufferSize)))
		{
			LeaveCriticalSection(&cs);
			printf("[!] malloc failed to build signals buffer in CreateGraph()\n");
			return NULL;
		}

		if (NULL == (pDATA->Xnorm = (double*)malloc(sizeof(double) * BufferSize)))
		{
			LeaveCriticalSection(&cs);
			printf("[!] malloc failed to build signals buffer in CreateGraph()\n");
			return NULL;
		}

		if (NULL == (pDATA->Ynorm = (double*)malloc(sizeof(double) * BufferSize)))
		{
			LeaveCriticalSection(&cs);
			printf("[!] malloc failed to build signals buffer in CreateGraph()\n");
			return NULL;
		}

		memset(pDATA->X, 0, sizeof(double) * BufferSize);
		memset(pDATA->Y, 0, sizeof(double) * BufferSize);
		memset(pDATA->Xnorm, 0, sizeof(double) * BufferSize);
		memset(pDATA->Ynorm, 0, sizeof(double) * BufferSize);

		pDATA->Xmax = 0.0;
		pDATA->Xmin = 0.0;
		pDATA->Ymax = 0.0;
		pDATA->Ymin = 0.0;

		pDATA->stat.min_value = 0.0;
		pDATA->stat.average_value_accumulator = 0.0;
		pDATA->stat.average_value_counter = 0;
		pDATA->stat.average_value = 0.0;
		pDATA->stat.max_value = 0.0;

			// set default signal name

		snprintf(pDATA->signame, sizeof(pDATA->signame) - 1, "Analog%i", i);

			// set default signal color

		pDATA->color[0] = 0.5f; pDATA->color[1] = 0.5f; pDATA->color[2] = 0.01f*i;

			// Update the number of signal inside object HGRAPH

		pgraph->signalcount++;
	}

	pgraph->totalsignalcount = pgraph->signalcount;
	pgraph->BufferSize = BufferSize;

		// Allocate a temp struct for computing

	SnapPlot = NULL;
	if(NULL== (SnapPlot = (DATA*)malloc(sizeof(DATA))))
	{
		LeaveCriticalSection(&cs);
		printf("[!] malloc failed to build temp signals buffer in CreateGraph()\n");
		return NULL;
	}

	if (NULL == (SnapPlot->X = (double*)malloc(sizeof(double) * BufferSize)))
	{
		LeaveCriticalSection(&cs);
		printf("[!] malloc failed to build temp signals buffer in CreateGraph()\n");
		return NULL;
	}

	if (NULL == (SnapPlot->Y = (double*)malloc(sizeof(double) * BufferSize)))
	{
		LeaveCriticalSection(&cs);
		printf("[!] malloc failed to build temp signals buffer in CreateGraph()\n");
		return NULL;
	}

	if (NULL == (SnapPlot->Xnorm = (double*)malloc(sizeof(double) * BufferSize)))
	{
		LeaveCriticalSection(&cs);
		printf("[!] malloc failed to build temp signals buffer in CreateGraph()\n");
		return NULL;
	}

	if (NULL == (SnapPlot->Ynorm = (double*)malloc(sizeof(double) * BufferSize)))
	{
		LeaveCriticalSection(&cs);
		printf("[!] malloc failed to build temp signals buffer in CreateGraph()\n");
		return NULL;
	}

	memset(SnapPlot->X, 0, sizeof(double) * BufferSize);
	memset(SnapPlot->Y, 0, sizeof(double) * BufferSize);
	memset(SnapPlot->Xnorm, 0, sizeof(double) * BufferSize);
	memset(SnapPlot->Ynorm, 0, sizeof(double) * BufferSize);

	pgraph->hParentWnd = hWnd;

	// Graph created in a "Static" control windows class named ""
	// When redrawn the control will be painted with the graph in place of

	//printf("[*] CreateGraph() of position l:%i t:%i r:%i b:%i \n", GraphArea.left, GraphArea.top, GraphArea.right, GraphArea.bottom );

	pgraph->hGraphWnd = CreateWindow(
		"Static",													// Predefined class; Unicode assumed 
		"",															// The text will be erased by OpenGL
		WS_EX_TRANSPARENT | WS_VISIBLE | WS_CHILD,					// Styles WS_EX_TRANSPARENT mandatory
		GraphArea.left,												// x pos
		GraphArea.top,												// y pos
		GraphArea.right,											// Graph width
		GraphArea.bottom,											// Graph height
		hWnd,														// Parent window
		NULL,														// No menu.
		(HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),			// HINST of the app
		NULL);														// No parameters

		// Sanity check

	if (NULL == pgraph->hGraphWnd)
	{
		printf("[!] CreateWindow() failed in CreateGraph()\n");
		LeaveCriticalSection(&cs);
		return NULL;
	}

	pgraph->hDC = GetDC(pgraph->hGraphWnd);

		// Sanity check

	if (NULL == pgraph->hDC)
	{
		printf("[!] GetDC() failed in CreateGraph()\n");
		LeaveCriticalSection(&cs);
		return NULL;
	}
		

		// Load OpenGL specific to legacy version

	ZeroMemory(&pfd, sizeof(pfd));
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 24;

		// To support advanced pixel format it is needed to load modern OpenGL functions
		// Only the legacy version is supported here

	PFDID = ChoosePixelFormat(pgraph->hDC, &pfd);
	if (PFDID == 0)
	{
		LeaveCriticalSection(&cs);
		MessageBox(0, "[!] Can't Find A Suitable PixelFormat.", "Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(0);
		return NULL;
	}

	if (SetPixelFormat(pgraph->hDC, PFDID, &pfd) == false)
	{
		LeaveCriticalSection(&cs);
		MessageBox(0, "[!] Can't Set The PixelFormat.", "Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(0);
		return NULL;
	}

		// Rendering Context

	pgraph->hRC = wglCreateContext(pgraph->hDC);

	if (pgraph->hRC == 0)
	{
		LeaveCriticalSection(&cs);
		MessageBox(0, "[!] Can't Create A GL Rendering Context.", "Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(0);
		return NULL;
	}

	if (wglMakeCurrent(pgraph->hDC, pgraph->hRC) == false)
	{
		LeaveCriticalSection(&cs);
		MessageBox(0, "[!] Can't activate GLRC.", "Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(0);
		return NULL;
	}
	pgraph->scale_factor = 1;
	pgraph->bAutoscale = true;
	pgraph->bDisplayCursor = true;
	pgraph->Logging = LOGGER_NONE;
	pgraph->Filtering = FILTER_NONE;
	GetClientRect(pgraph->hGraphWnd, &DispArea);
	InitGL(pgraph, DispArea.right, DispArea.bottom);
	ReshapeGraph(pgraph, DispArea.left, DispArea.top, DispArea.right, DispArea.bottom );
	LeaveCriticalSection(&cs);
	return pgraph;
}

/*-------------------------------------------------------------------------
	SetSignalCount: set a new total signal count ; 
	must be in range between [0;MAX_SIGNAL_COUNT]
-------------------------------------------------------------------------*/

BOOL SetSignalCount(HGRAPH hGraph,CONST INT iSignalNumber)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetSignalCount() graph handle is null\n");
		return FALSE;
	}

	if (iSignalNumber > MAX_SIGNAL_COUNT)
	{
		printf("[!] Error at SetSignalCount()\n%i signal max is reached\n", pgraph->signalcount);
	    return FALSE;
	}

	pgraph->signalcount = iSignalNumber;
	return TRUE;
}

/*-------------------------------------------------------------------------
	SetSignalLabel: set a name to a signal [0;MAXSIG-1]
-------------------------------------------------------------------------*/

VOID SetSignalLabel(HGRAPH hGraph, CONST CHAR szLabel[260], INT iSignalNumber) 
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetSignalLabel()\ngraph handle is null\n");
		return;
	}

	if (iSignalNumber < 0 || iSignalNumber >= pgraph->signalcount)
	{
		printf("[!] Error at SetSignalLabel()\nrange must be [1;%i] and is %i\n", pgraph->signalcount, iSignalNumber);
		return;
	}

	printf("[*] a new signal name is assigned: %s\n", szLabel);
	DATA* signal = (DATA*)pgraph->signal[iSignalNumber];
	strncpy_s(signal->signame, szLabel, sizeof(signal->signame)-1);
}

/*-------------------------------------------------------------------------
	SetSignalColor: Specify a RGB color [0-255] 
-------------------------------------------------------------------------*/

VOID SetSignalColor(HGRAPH hGraph, INT R, INT G, INT B, INT iSignalNumber)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetSignalColor() graph handle is null\n");
		return;
	}

	if(iSignalNumber> MAX_SIGNAL_COUNT-1)
	{
		printf("[!] Error at SetSignalColor() MAX_SIGNAL_COUNT reached \n");
		return;
	}

	if (R < 0 || R> 255)
	{
		printf("[!] Error at SetSignalColor() R value overflow R:%i\n", R);
		return;
	}

	if (G < 0 || G> 255)
	{
		printf("[!] Error at SetSignalColor() G value overflow G:%i\n", G);
		return;
	}

	if (B < 0 || B> 255)
	{
		printf("[!] Error at SetSignalColor() B value overflow B:%i\n", B);
		return;
	}

	printf("[*] a new signal color is assigned: RGBf (%i %i %i) at position: %i\n", R,G,B, iSignalNumber);
	DATA* signal = (DATA*)pgraph->signal[iSignalNumber];
	if (NULL == signal)
	{
		printf("[!] Error at SetSignalColor() graph signal %i is null\n", iSignalNumber);
		return;
		
	}
	signal->color[0] = (float)R / 255.0f;
	signal->color[1] = (float)G / 255.0f;
	signal->color[2] = (float)B / 255.0f;


	//////////////////////////////////////////////////

	for (int i = 0; i < pgraph->signalcount; i++)
	{
		signal = (DATA*)pgraph->signal[i];
		printf("[WINGRAPH]%i %s (%i %i %i)\n",i, signal->signame, (int)(signal->color[0]*255.0f), (int)(signal->color[1] * 255.0f), (int)(signal->color[2] * 255.0f));	
	}
}

/*-------------------------------------------------------------------------
	SetSignalVisible: Enable or disable specific signal on graph
-------------------------------------------------------------------------*/

VOID SetSignalVisible(HGRAPH hGraph, BOOL bDisplay, INT iSignalNumber)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetSignalVisible()\ngraph handle is null\n");
		return;
	}

	if (iSignalNumber < 0 || iSignalNumber >= pgraph->signalcount)
	{
		printf("[!] Error at SetSignalVisible()\nrange must be [1;%i] and is %i\n", pgraph->signalcount, iSignalNumber);
		return;
	}

	DATA* signal = (DATA*)pgraph->signal[iSignalNumber];
	signal->show = bDisplay;

	printf("[*] Signal visibility changed at signal number: %i\n", iSignalNumber);
}

/*-------------------------------------------------------------------------
	SetRecordingMode: set the graph reccording state with bLogging
  -------------------------------------------------------------------------*/

VOID SetRecordingMode(HGRAPH hGraph, LOGGER_M logging)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetRecordingMode() graph handle is null\n");
		return;
	}
	pgraph->Logging = logging;
}

/*-------------------------------------------------------------------------
	SetAutoscaleMode: set autoscale
  -------------------------------------------------------------------------*/

VOID SetAutoscaleMode(HGRAPH hGraph, BOOL mode)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetAutoscaleMode() graph handle is null\n");
		return;
	}

	pgraph->bAutoscale = mode;

	if (mode == FALSE)
	{
		if (pgraph->bRunning)
		{
			DATA* pData = NULL;
			pData = (DATA * )pgraph->signal[0];

			// TODO Check every signal not just once
			// only chan 1 is evaluated
			pgraph->ymax_fix = pData->Ymax;
			pgraph->ymin_fix = pData->Ymin;
			pgraph->xwindow_fix = pData->X[pgraph->cur_nbpoints-1];
		}
		
	}
}

/*-------------------------------------------------------------------------
	SetDisplayCursor: Add indicator bellow the mouse cursor with X and Y values
  -------------------------------------------------------------------------*/

VOID SetDisplayCursor(HGRAPH hGraph, BOOL isActive)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetDisplayCursor() graph handle is null\n");
		return;
	}

	pgraph->bDisplayCursor = isActive;
}

/*-------------------------------------------------------------------------
	SetYminVal: set the Ymin scale value
  -------------------------------------------------------------------------*/

VOID SetYminVal(HGRAPH hGraph, double ymin)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetYminVal() graph handle is null\n");
		return;
	}

	pgraph->ymin_fix = ymin;
}

/*-------------------------------------------------------------------------
	SetYmaxVal: set the Ymin scale value
  -------------------------------------------------------------------------*/

VOID SetYmaxVal(HGRAPH hGraph, double ymax)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetYmaxVal() graph handle is null\n");
		return;
	}

	pgraph->ymax_fix = ymax;
}

VOID SetZoomFactor(HGRAPH hGraph, int zoom)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetZoomFactor() graph handle is null\n");
		return;
	}

	if (zoom < 0)
	{
		printf("[!] Error at SetZoomFactor() zoom can't be <0\n");
		return;
	}

	pgraph->scale_factor = zoom;
	return;
}

/*-------------------------------------------------------------------------
	SetFilteringMode: set the EMA filtering state
  -------------------------------------------------------------------------*/

VOID SetFilteringMode(HGRAPH hGraph, FILTER_M filtering)
{		
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetFilteringMode() graph handle is null\n");
		return;
	}

	pgraph->Filtering = filtering;
}

VOID SetSignalMinValue(HGRAPH hGraph, INT SIGNB, DOUBLE val)
{
	if (SIGNB > MAX_SIGNAL_COUNT || SIGNB < 0)
	{
		printf("[!] Error at SignalResetStatisticValue() signal number not in range\n");
		return;
	}

	EnterCriticalSection(&cs);
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SignalResetStatisticValue() graph handle is null\n");
		LeaveCriticalSection(&cs);
		return;
	}

	if (NULL == pgraph->signalcount)
	{
		printf("[!] Error at SignalResetStatisticValue() graph signal count is null\n");
		LeaveCriticalSection(&cs);
		return;
	}

	DATA* pData = NULL;
	pData = (DATA*)pgraph->signal[SIGNB];
	if (pgraph->cur_nbpoints >= 0)
	{
		pData->stat.min_value = val;
		pData->stat.average_value_accumulator = 0.0;
		pData->stat.average_value_counter = 0;
	}
	LeaveCriticalSection(&cs);
}

VOID SetSignalAverageValue(HGRAPH hGraph, INT SIGNB, DOUBLE val)
{
	if (SIGNB > MAX_SIGNAL_COUNT || SIGNB < 0)
	{
		printf("[!] Error at SignalResetStatisticValue() signal number not in range\n");
		return;
	}

	EnterCriticalSection(&cs);
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SignalResetStatisticValue() graph handle is null\n");
		LeaveCriticalSection(&cs);
		return;
	}

	if (NULL == pgraph->signalcount)
	{
		printf("[!] Error at SignalResetStatisticValue() graph signal count is null\n");
		LeaveCriticalSection(&cs);
		return;
	}

	DATA* pData = NULL;
	pData = (DATA*)pgraph->signal[SIGNB];
	if (pgraph->cur_nbpoints >= 0)
	{
		pData->stat.average_value = val;
		pData->stat.average_value_accumulator = 0.0;
		pData->stat.average_value_counter = 0;

	}
	LeaveCriticalSection(&cs);
}

VOID SetSignalMaxValue(HGRAPH hGraph, INT SIGNB, DOUBLE val)
{
	if (SIGNB > MAX_SIGNAL_COUNT || SIGNB < 0)
	{
		printf("[!] Error at SignalResetStatisticValue() signal number not in range\n");
		return;
	}

	EnterCriticalSection(&cs);
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SignalResetStatisticValue() graph handle is null\n");
		LeaveCriticalSection(&cs);
		return;
	}

	if (NULL == pgraph->signalcount)
	{
		printf("[!] Error at SignalResetStatisticValue() graph signal count is null\n");
		LeaveCriticalSection(&cs);
		return;
	}

	DATA* pData = NULL;
	pData = (DATA*)pgraph->signal[SIGNB];
	if (pgraph->cur_nbpoints >= 0)
	{
		pData->stat.average_value_accumulator = 0.0;
		pData->stat.average_value_counter = 0;
		pData->stat.max_value = val;
	}
	LeaveCriticalSection(&cs);
}

/*-------------------------------------------------------------------------
	GetGraphRC: return the graph state from bRunning
  -------------------------------------------------------------------------*/

BOOL GetGraphState(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetGraphState() graph handle is null\n");
		return FALSE;
	}

	return pgraph->bRunning;
}

/*-------------------------------------------------------------------------
	GetGraphRC: return the graph render context
  -------------------------------------------------------------------------*/

HGLRC GetGraphRC(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetGraphRC() graph handle is null\n");
		return NULL;
	}

	if (NULL == pgraph->hRC)
	{
		printf("[!] Error at GetGraphRC() graph RC is null\n");
		return NULL;
	}
	return pgraph->hRC;
}

/*-------------------------------------------------------------------------
	GetGraphDC: return the graph device context
  -------------------------------------------------------------------------*/

HDC GetGraphDC(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetGraphDC() graph handle is null\n");
		return NULL;
	}

	if (NULL == pgraph->hDC)
	{
		printf("[!] Error at GetGraphDC() graph DC is null\n");
		return NULL;
	}

	return pgraph->hDC;
}

/*-------------------------------------------------------------------------
	GetGraphParentWnd: return the graph parent HWND
  -------------------------------------------------------------------------*/

HWND GetGraphParentWnd(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetGraphParentWnd() graph handle is null\n");
		return NULL;
	}

	if (NULL == pgraph->hParentWnd)
	{
		printf("[!] Error at GetGraphDC() graph parent windows is null\n");
		return NULL;
	}

	return pgraph->hParentWnd;
}

/*-------------------------------------------------------------------------
	GetGraphWnd: return the graph HWND
  -------------------------------------------------------------------------*/

HWND GetGraphWnd(HGRAPH hGraph)
{	
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetGraphWnd() graph handle is null\n");
		return NULL;
	}

	if (NULL == pgraph->hGraphWnd)
	{
		printf("[!] Error at GetGraphWnd() graph Windows is null\n");
		return NULL;
	}

	return pgraph->hGraphWnd;
}

/*-------------------------------------------------------------------------
	GetGraphSignalNumber: return the total signals number
  -------------------------------------------------------------------------*/

INT GetGraphSignalCount(HGRAPH hGraph)
{		
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetGraphSignalCount() graph handle is null\n");
		return -1;
	}

	if (NULL == pgraph->signalcount)
	{
		printf("[!] Error at GetGraphSignalCount() graph signal count is null\n");
		return -1;
	}

	return pgraph->signalcount;
}

INT GetZoomFactor(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SetZoomFactor() graph handle is null\n");
		return 0;
	}

	return pgraph->scale_factor;
}

/*-------------------------------------------------------------------------
	GetGraphSignalNumber: return the total signals number
  -------------------------------------------------------------------------*/

double GetGraphLastSignalValue(HGRAPH hGraph, INT SIGNB)
{
	EnterCriticalSection(&cs);
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetGraphLastSignalValue() graph handle is null\n");
		LeaveCriticalSection(&cs);
		return 0.0f;
	}

	if (NULL == pgraph->signalcount)
	{
		printf("[!] Error at GetGraphLastSignalValue() graph signal count is null\n");
		LeaveCriticalSection(&cs);
		return 0.0f;
	}

	DATA* pData = NULL;
	pData = (DATA*)pgraph->signal[SIGNB];
	if (pgraph->cur_nbpoints > 0)
	{
		LeaveCriticalSection(&cs);
		return pData->Y[pgraph->cur_nbpoints - 1];
	}
	else
	{
		LeaveCriticalSection(&cs);
		return 0.0f;
	}		
}

double GetSignalMinValue(HGRAPH hGraph, INT SIGNB)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetSignalMinValue() graph handle is null\n");
		return 0.0f;
	}

	if (NULL == pgraph->signalcount)
	{
		printf("[!] Error at GetSignalMinValue() graph signal count is null\n");
		return 0.0f;
	}

	if (SIGNB > MAX_SIGNAL_COUNT || SIGNB < 0)
	{
		printf("[!] Error at SignalResetStatisticValue() signal number not in range\n");
		return 0.0;
	}

	DATA* pData = NULL;
	pData = (DATA*)pgraph->signal[SIGNB];
	if (pgraph->cur_nbpoints > 0)
	{
		return pData->stat.min_value;
	}
	return 0.0;
}

double GetSignalAverageValue(HGRAPH hGraph, INT SIGNB)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetSignalAverageValue() graph handle is null\n");
		return 0.0f;
	}

	if (NULL == pgraph->signalcount)
	{
		printf("[!] Error at GetSignalAverageValue() graph signal count is null\n");
		return 0.0f;
	}

	if (SIGNB > MAX_SIGNAL_COUNT || SIGNB < 0)
	{
		printf("[!] Error at SignalResetStatisticValue() signal number not in range\n");
		return 0.0;
	}

	DATA* pData = NULL;
	pData = (DATA*)pgraph->signal[SIGNB];
	if (pgraph->cur_nbpoints > 0)
	{
		return pData->stat.average_value;
	}
	return 0.0;
}

double GetSignalMaxValue(HGRAPH hGraph, INT SIGNB)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetSignalMaxValue() graph handle is null\n");
		return 0.0f;
	}

	if (NULL == pgraph->signalcount)
	{
		printf("[!] Error at GetSignalMaxValue() graph signal count is null\n");
		return 0.0f;
	}

	if (SIGNB > MAX_SIGNAL_COUNT || SIGNB < 0)
	{
		printf("[!] Error at SignalResetStatisticValue() signal number not in range\n");
		return 0.0;
	}

	DATA* pData = NULL;
	pData = (DATA*)pgraph->signal[SIGNB];
	if (pgraph->cur_nbpoints > 0)
	{
		return pData->stat.max_value;
	}
	return 0.0;
}

VOID SignalResetStatisticValue(HGRAPH hGraph, INT SIGNB)
{
	if (SIGNB > MAX_SIGNAL_COUNT || SIGNB < 0)
	{
		printf("[!] Error at SignalResetStatisticValue() signal number not in range\n");
		return;
	}

	EnterCriticalSection(&cs);
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at SignalResetStatisticValue() graph handle is null\n");
		LeaveCriticalSection(&cs);
		return;
	}

	if (NULL == pgraph->signalcount)
	{
		printf("[!] Error at SignalResetStatisticValue() graph signal count is null\n");
		LeaveCriticalSection(&cs);
		return;
	}

	DATA* pData = NULL;
	pData = (DATA*)pgraph->signal[SIGNB];
	if (pgraph->cur_nbpoints > 0)
	{	
		pData->stat.min_value = pData->Y[pgraph->cur_nbpoints - 1];
		pData->stat.average_value = pData->Y[pgraph->cur_nbpoints - 1];
		pData->stat.average_value_accumulator = 0.0;
		pData->stat.average_value_counter = 0;
		pData->stat.max_value = pData->Y[pgraph->cur_nbpoints - 1];
		printf("[*] SignalResetStatisticValue() min: %lf	avg: %lf	max: %lf\n", pData->stat.min_value, pData->stat.average_value, pData->stat.max_value);
	}
	LeaveCriticalSection(&cs);
}


VOID AddPoints(HGRAPH hGraph, double* y, INT PointsCount)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at AddPoints() graph handle is null\n");
		return;
	}

	DATA* pDATA = NULL;
	if (cs.DebugInfo == NULL)
	{
		printf("[!] Error at AddPoints() critical section not available\n");
		return;
	}

	EnterCriticalSection(&cs);

		// Sanity check

	if (FALSE == pgraph->bRunning)
	{
		LeaveCriticalSection(&cs);
		printf("[!] Error at AddPoints() graph not strated\n");
		return;
	}

		// TODO: Check if signalcount = length of y!

	if (pgraph->signalcount != PointsCount)
	{
		LeaveCriticalSection(&cs);
		printf("[!] Error at AddPoints() signalcount not egual to y length\n");
		return;
	}

	// If the maximum points are reached 
	// in the buffer, shift left the array and
	// dec the current number of points

	if (pgraph->cur_nbpoints == pgraph->BufferSize)
	{
		for (int index = 0; index < pgraph->signalcount; index++)
		{
			pDATA = pgraph->signal[index];
			for (int j = 0; j < pgraph->BufferSize - 1; j++)
			{
				pDATA->X[j] = pDATA->X[j + 1];																// Shift left X
				pDATA->Y[j] = pDATA->Y[j + 1];																// Shift left Y
			}
		}
		pgraph->cur_nbpoints--;																				// Update the current point number
	}

		// Save the actual timestamp

	if (pgraph->cur_nbpoints == 0)
	{
		finish = start;
	}
	else
	{
		finish = PerformanceCounter();
	}
	if (pgraph->Logging == LOGGER_ASCII)
	{
		if (logfile)
		{
			fprintf(logfile, "%lf\t", (double)((finish - start)) / frequency);
		}
	}
	if (pgraph->Logging == LOGGER_XLSX)
	{
		if (XL)
		{
			char buffer[260] = "";
			sprintf_s(buffer, "%lf\t", (double)((finish - start)) / frequency);
			excel_addline(&XL, buffer);
		}
	}

	char lpszDataValues[260] = ""; // char values accumulator for XLSX; be carefull max 260 char -> BOF

	for (int index = 0; index < pgraph->signalcount; index++)
	{
		pDATA = pgraph->signal[index];
		if (NULL == pDATA)
		{
			LeaveCriticalSection(&cs);
			printf("[!] Error at AddPoints() data buffer is null\n");
			return;
		}

			// Low pass filter 

		if (pgraph->Filtering == FILTER_EMA)
		{
			double a = 0.1; // Custom cut freq 
			if (pgraph->cur_nbpoints == 0)
				y[index] = y[index]; // First point skip to prevent INF
			else
				y[index] = a * y[index] + (1 - a) * pDATA->Y[pgraph->cur_nbpoints - 1];						// Low pass filter EMA "f(x) = x1 * a + (1-a) * x0" where a [0;1]
		}

			// Hanning window filter

		if (pgraph->Filtering == FILTER_HANNING) // experimental (not properly working)
		{
			const int window_size = 20;																		// Define the Hann windows size here
			static double dataOut[window_size];
			static int accumulator = 0;
			if (accumulator < window_size)
			{
				printf("[*] Hanning filter collecting...\n");
				LeaveCriticalSection(&cs);
				accumulator++;
				return;
			}
			accumulator = 0;

			for (int i = 0; i < window_size; i++)
			{			
				
				const double PI = 3.14159;
				double multiplier = 0.5 * (1 - cos(2 * PI * i / window_size));
				dataOut[i] = multiplier * y[index];
			}
		}

			// Besel filter

		if (pgraph->Filtering == FILTER_BESEL)
		{
		}
			
			// Add points to the selected buffer	

		pDATA->X[pgraph->cur_nbpoints] = (double)((finish - start)) / frequency;							// Save in X the elapsed time from start
		pDATA->Y[pgraph->cur_nbpoints] = y[index];															// Save Y

			// Perform some statistics

		// period
		pDATA->stat.period_s = pDATA->X[pgraph->cur_nbpoints] - pDATA->X[0];								// Update current period
		
		//min
		if (pDATA->Y[pgraph->cur_nbpoints] < pDATA->stat.min_value)
		{
			pDATA->stat.min_value = pDATA->Y[pgraph->cur_nbpoints];											// Update current min value displayed
		}

		//average
		if (pgraph->cur_nbpoints > 0)
		{
			pDATA->stat.average_value_accumulator += pDATA->Y[pgraph->cur_nbpoints];
			pDATA->stat.average_value_counter++;

			pDATA->stat.average_value = pDATA->stat.average_value_accumulator / pDATA->stat.average_value_counter;
		}

		//max
		if (pDATA->Y[pgraph->cur_nbpoints] > pDATA->stat.max_value)
		{
			pDATA->stat.max_value = pDATA->Y[pgraph->cur_nbpoints];											// Update current max value displayed
		}
		if (pgraph->Logging == LOGGER_ASCII)
		{
			if (logfile)
			{
				fprintf_s(logfile, "%lf\t", y[index]);
			}
		}
		if (pgraph->Logging == LOGGER_XLSX)
		{
			if (XL)
			{
				sprintf_s(lpszDataValues, "%lf\t", y[index]);
			}
		}
	}
	if (pgraph->Logging == LOGGER_ASCII)
	{
		if (logfile)
		{
			fprintf(logfile, "\n");
		}
	}
	if (pgraph->Logging == LOGGER_XLSX)
	{
		if (XL)
		{
			HRESULT hres = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			excel_addline(XL, lpszDataValues);
		}
	}


		// Inc the number of points

	pgraph->cur_nbpoints++;

	LeaveCriticalSection(&cs);
}

/*-------------------------------------------------------------------------
	Render: Analyze the data buffers and print waves to the
	OpenGL device context
  -------------------------------------------------------------------------*/
BOOL Render(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at Render() graph handle is null\n");
		return FALSE;
	}

	if (NULL == pgraph->hGraphWnd)
	{
		printf("[!] Error at Render() graph windows is null\n");
		return FALSE;
	}

	RECT r;
	const int div = 10;
	float txtlen = 0.0;
	float txtheight = 0.0;
	char value[260];
	const char Xname[] = "Time (s)";
	double reelval = SnapPlot->Ymin;

	EnterCriticalSection(&cs);

	if (pgraph->cur_nbpoints == 0 && start == 0.0f)														// Display a void graph when app start only
	{
		SnapPlot->Xmin = 0.0f;
		SnapPlot->Xmax = 1.0f;
		SnapPlot->Ymin = 0.0f;
		SnapPlot->Ymax = 1.0f;
	}
	else if (pgraph->cur_nbpoints > 0)
	{	
		UpdateBorder(hGraph);																			// border determination for each signal: meaning finding X and Y min max values																		
		ZeroObject(SnapPlot, GetBufferSize(hGraph));													// Clear SnapPlot
		FindGlobalMaxScale(hGraph, SnapPlot->Xmin, SnapPlot->Xmax, SnapPlot->Ymin, SnapPlot->Ymax);		// Load the Y min and max of all the signals in SnapPlot
		normalize_data(hGraph, SnapPlot->Xmin, SnapPlot->Xmax, SnapPlot->Ymin, SnapPlot->Ymax);			// normalize between [0;1]
	}

	GetClientRect(pgraph->hGraphWnd, &DispArea);

		// Use the Projection Matrix

	glMatrixMode(GL_PROJECTION);

		// Reset Matrix

	glLoadIdentity();

		// Set the correct perspective.

	//gluOrtho2D(-0.08, 1.04, 0 - 0.08, 1 + 0.02);

	const float orthoLeft = -0.08f; const float orthoRight = 1.04f;
	const float orthoBottom = -0.08f; const float orthoTop = 1.02f;
	gluOrtho2D(orthoLeft, orthoRight, orthoBottom, orthoTop);

	glViewport(0, 0, DispArea.right, DispArea.bottom);

		// Use the Model Matrix

	glMatrixMode(GL_MODELVIEW);

		// Reset Matrix

	glLoadIdentity();

		// Clear Color and Depth Buffers

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Draw graph frame and grid

	DrawGraphSquare();
	DrawGridLines();

	if (SnapPlot->Ymax != SnapPlot->Ymin)
	{

			// draw points
	
		DrawWave(hGraph);

			// draw indicators

		glColor3f(0.1f, 0.1f, 0.1f);

			// draw zero cursor

		double zero = GetStandardizedData(0, SnapPlot->Ymin, SnapPlot->Ymax);
		if (zero > 0 && zero < 1)
		{
			DrawString(-0.02f, (float)zero - 0.01f, (char*)"0");
			DrawCursor(0.0f, (float)zero);
		}

			// Determine the length of a typical string for resizing purpose

		GetWindowRect(GetGraphWnd(hGraph), &r);
		txtlen = (float)dispStringWidth.cx / r.right; // Normalize the width of the printed value characters between [0-1]
		txtheight = (float)dispStringWidth.cy / r.bottom;

			//Xmin

		DrawString(-txtlen / 1.2f, -0.05f, dtos(value, sizeof(value), SnapPlot->Xmin));

			//Xmax

		DrawString(1 - txtlen / 1.2f, -0.05f, dtos(value, sizeof(value), SnapPlot->Xmax));

			//Time (s)

		DrawString(0.5f - (txtlen / 2.0f), -0.05f, (char*)Xname);

			// Ymin to Ymax values

		for (float ytmp = 0.0; ytmp <= 1.1; ytmp += 1.0 / (float)div)
		{
			DrawString(-txtlen * 1.5f, ytmp - ((txtheight * 0.8f) / 2.0f), dtos(value, sizeof(value), reelval));
			reelval += (SnapPlot->Ymax - SnapPlot->Ymin) / (float)div;
		}
	}

		// Display cursor with values if hoover

	if (pgraph->bDisplayCursor)
	{
		//DisplayPointer(hGraph);

		if (isMouseHover(hGraph))
		{
			POINT pos = GetMousePosition();
			/////////////////////////////
			RECT client;
			HWND hwndGraph = NULL;
			hwndGraph = GetGraphWnd(hGraph);
			if (GetWindowRect(hwndGraph, &client))
			{
				// Normalization [0-1] on client area
				// Xnorm = (X - min) / (max - min);
				float normal_pos_x = ((float)pos.x - (float)client.left) / ((float)client.right - (float)client.left);
				float normal_pos_y = ((float)pos.y - (float)client.bottom) / ((float)client.top - (float)client.bottom);
			
				// Normalization [0-1] on graph area
				float normal_pos_x_shifted = ((float)normal_pos_x - (float)0.07) / ((float)0.965 - (float)0.07);
				float normal_pos_y_shifted = ((float)normal_pos_y - (float)0.07) / ((float)0.98 - (float)0.07);

				if ((normal_pos_y_shifted < 1.0f) && (normal_pos_y_shifted > 0.0f))
				{
					if ((normal_pos_x_shifted < 1.0f) && (normal_pos_x_shifted > 0.0f))
					{
						// Display X indicator

						//DrawString(normal_pos_x, normal_pos_y - 0.05, dtos(value, sizeof(value), (normal_pos_x_shifted * (SnapPlot->Xmax - SnapPlot->Xmin)) + SnapPlot->Xmin));

						// Display Y indicator

						double dy = 0.0;
						dy = (SnapPlot->Ymin - SnapPlot->Ymax) * (1 - normal_pos_y_shifted);
						dy = SnapPlot->Ymax + dy;
						DrawString(normal_pos_x + 0.05, normal_pos_y - 0.05, dtos(value, sizeof(value), dy));

						printf("x=%lf	y=%lf	xnorm=%lf	ynorm=%lf\n", normal_pos_x, normal_pos_y, normal_pos_x_shifted, normal_pos_y_shifted);

						// Display lines indicator

						/*
						glColor3f(0.0f, 1.0f, 0.5f);
						glBegin(GL_LINE_STRIP);
						glVertex2f(0, 0);
						glVertex2f(normal_pos_x_shifted, normal_pos_y_shifted);
						glEnd();
						*/
						glColor3f(0.0f, 0.4f, 0.5f);
						glBegin(GL_LINE_STRIP);
						glVertex2f(0, normal_pos_y_shifted);
						glVertex2f(1, normal_pos_y_shifted);
						glEnd();

						glBegin(GL_LINE_STRIP);
						glVertex2f(normal_pos_x_shifted, 0);
						glVertex2f(normal_pos_x_shifted, 1);
						glEnd();

						// OK
						//DrawString(normal_pos_x, normal_pos_y - 0.05, dtos(value, sizeof(value), (normal_pos_x * (SnapPlot->Xmax - SnapPlot->Xmin)) + SnapPlot->Xmin));


						//~OK
						//double dy = 0.0;
						//dy = (SnapPlot->Ymin - SnapPlot->Ymax)* (1-normal_pos_y);
						//dy = SnapPlot->Ymax + dy;
						//printf("dy:%lf, ymin:%lf ymax%lf\n", dy, SnapPlot->Ymin, SnapPlot->Ymax);


						// OK
						//DrawString(normal_pos_x + 0.05, normal_pos_y - 0.05, dtos(value, sizeof(value), dy));
					}
				}
			}
		}
	}
	LeaveCriticalSection(&cs);
	SwapBuffers(GetGraphDC(hGraph));
	return TRUE;
}

/*-------------------------------------------------------------------------
	ReshapeGraph: When resize message is proceed update graph pos
  -------------------------------------------------------------------------*/

VOID ReshapeGraph(HGRAPH hGraph, int left, int top, int right, int bottom)
{
		// Sanity check

	if (NULL == hGraph)
	{
		printf("[!] Error at ReshapeGraph() graph handle is null\n");
		return;
	}	

	HWND hitem = GetGraphWnd(hGraph);
	if (SetWindowPos(hitem, NULL, left, top, right, bottom, SWP_SHOWWINDOW))
	{
		SetGLView(right, bottom);
	}
	else
	{
		printf("    [!] Error at SetWindowPos() with code: 0x%x\n", GetLastError());
	}
}

/*-------------------------------------------------------------------------
	InitGL: Setup the font used, set the correct OpenGL view at init
  -------------------------------------------------------------------------*/

VOID InitGL(HGRAPH hGraph, int Width, int Height)		// Called after the main window is created
{
		// Sanity check

	if (NULL == hGraph)
	{
		printf("[!] Error at InitGL() graph handle is null\n");
		return;
	}

	//glShadeModel(GL_SMOOTH);							// Enable Smooth drawing MSAA...
	//glEnable(GL_LINE_SMOOTH);							// Enable Smooth drawing MSAA...
	//glEnable(GL_BLEND);
	glDepthMask(false);

	if (!BuildMyFont(hGraph, (char*)"Verdana", 12))		// Build The Font BuildMyFont(HGRAPH hGraph, char* FontName, int Fontsize)
	{
		// error on creating the Font
	}
	SetGLView(Width, Height);
}

/*-------------------------------------------------------------------------
	SetGLView: Make background and check for errors
  -------------------------------------------------------------------------*/

VOID SetGLView(int Width, int Height)
{
	glClearColor(0.98f, 0.98f, 0.98f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	//CheckErr();
}

/*-------------------------------------------------------------------------
	BuildFont: Create a Windows Bitmap Font to write the device context with
  -------------------------------------------------------------------------*/

BOOL BuildMyFont(HGRAPH hGraph, char * FontName, int Fontsize)
{
	HFONT hCustomFont = NULL;									// New font to create
	HFONT hCurrentFont = NULL;									// Store the current Windows font

		// Sanity check

	if (NULL == hGraph)
	{
		printf("[!] Error at BuildMyFont() graph handle is null\n");
		return FALSE;
	}

	// Create an empty display list of 96 char (We are using ASCII char from 32 to 127 only)

	base = glGenLists(96);	
	if (0 == base)
	{
		// error when generate the empty list
		printf("[!] Error at BuildMyFont() with code: 0x%x\nerror when generate the empty list\n", GetLastError());
		return FALSE;
	}

	hCustomFont = CreateFont(-1 * Fontsize,						// Font size
		0,														// Width Of Font
		0,														// Angle Of Escapement
		0,														// Orientation Angle
		FW_NORMAL,												// Font Weight
		FALSE,													// Italic
		FALSE,													// Underline
		FALSE,													// Strikeout
		ANSI_CHARSET,											// Character Set Identifier
		OUT_TT_PRECIS,											// Output Precision
		CLIP_DEFAULT_PRECIS,									// Clipping Precision
		ANTIALIASED_QUALITY,									// Output Quality
		FF_DONTCARE | DEFAULT_PITCH,							// Family And Pitch
		FontName);												// Font Name

	if (NULL == hCustomFont)
	{
		// error when creating the font
		printf("[!] Error at BuildMyFont() with code: 0x%x\nerror when creating the font\n", GetLastError());
		return FALSE;
	}

	hCurrentFont = (HFONT)SelectObject(GetGraphDC(hGraph), hCustomFont);	// Select the custom Font and store the current font

	if (!wglUseFontBitmaps(GetGraphDC(hGraph), 32, 96, base))				// Builds 96 Characters Starting At Character 32 and store it in the list
	{
		// error when loading the font
		printf("[!] Error at BuildMyFont() with code: 0x%x\nerror when loading the font\n", GetLastError());
		return FALSE;
	}
	const char text[] = "-0.0000";
	SetTextCharacterExtra(GetGraphDC(hGraph), 1);
	GetTextExtentPoint32A(GetGraphDC(hGraph), text, strlen(text), &dispStringWidth);
	dispStringWidth.cx -= GetTextCharacterExtra(GetGraphDC(hGraph)) * (strlen(text) - 2);

	SelectObject(GetGraphDC(hGraph), hCurrentFont);							// restore the initial Font
	DeleteObject(hCustomFont);												// We don't need the Custom Font anymore as we populate the list and load it in OpenGL
	return TRUE;
}

/*-------------------------------------------------------------------------
	KillFont: Free the font from OpenGL
  -------------------------------------------------------------------------*/

void KillFont(GLvoid)														// Delete The Font List
{
	glDeleteLists(base, 96);												// Delete All 96 Characters ( NEW )
}

/*-------------------------------------------------------------------------
	glPrint: TBD
  -------------------------------------------------------------------------*/
  //https://nehe.gamedev.net/tutorial/bitmap_fonts/17002/

GLvoid glPrint(const char* fmt, ...)										// Custom GL "Print" Routine
{
	char        text[256];													// Holds Our String
	va_list     ap;															// Pointer To List Of Arguments

	if (fmt == NULL)														// If There's No Text
		return;																// Do Nothing

	va_start(ap, fmt);														// Parses The String For Variables
	vsprintf_s(text, fmt, ap);												// And Converts Symbols To Actual Numbers
	va_end(ap);																// Results Are Stored In Text
	glPushAttrib(GL_LIST_BIT);												// Pushes The Display List Bits     ( NEW )
	glListBase(base - 32);													// Sets The Base Character to 32    ( NEW )
	glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);						// Draws The Display List Text  ( NEW )
	glPopAttrib();															// Pops The Display List Bits   ( NEW )
}

/*-------------------------------------------------------------------------
	FindGlobalMaxScale: Scanning arrays to determine the border
  -------------------------------------------------------------------------*/

BOOL FindGlobalMaxScale(HGRAPH hGraph, double& Xmin, double& Xmax, double& Ymin, double& Ymax)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at FindGlobalMaxScale() graph handle is null\n");
		return FALSE;
	}

	if (0 != pgraph->signalcount)
	{
		int first_sig = 0;
		for (int index = 0; index < pgraph->signalcount; index++)			// find first displayed signal (show=true)
		{
			if (pgraph->signal[index]->show == false)
			{
				continue;
			}
			else
			{
				first_sig = index;											// save first signal number
				break;
			}
		}
		
		Xmin = pgraph->signal[first_sig]->Xmin;										// save first value
		Xmax = pgraph->signal[first_sig]->Xmax;										// save first value	
		Ymin = pgraph->signal[first_sig]->Ymin;										// save first value
		Ymax = pgraph->signal[first_sig]->Ymax;										// save first value


		for (int index = first_sig+1; index < pgraph->signalcount; index++)			// Iterate every signals
		{
			if (pgraph->signal[index]->show == true)						// Update limit only if signal displayed
			{
				if (Xmin > pgraph->signal[index]->Xmin)
					Xmin = pgraph->signal[index]->Xmin;							// Update if needed

				if (Xmax < pgraph->signal[index]->Xmax)
					Xmax = pgraph->signal[index]->Xmax;							// Update if needed

				if (Ymin > pgraph->signal[index]->Ymin)
					Ymin = pgraph->signal[index]->Ymin;							// Update if needed

				if (Ymax < pgraph->signal[index]->Ymax)
					Ymax = pgraph->signal[index]->Ymax;							// Update if needed
			}
		}
		
			// prevent div by 0

		if ((Ymax - Ymin) == 0)
		{
			Ymax += 0.5;
			Ymin -= 0.5;
		}

			// Optimize display with rounded units

		const double rangeMult[] = { /*0.1, 0.2, 0.25, 0.5, 1.0, */2.0, 2.5, 5.0};
		const int segnumber = 10;
		double magnitude = floor(log10(Ymax - Ymin));
		
		for (int i = 0; i < sizeof(rangeMult) / sizeof(double); i++)
		{
			double step_size = rangeMult[i] * pow(10, magnitude);
			double low = floor(Ymin / step_size) * step_size;
			double high = ceil(Ymax / step_size) * step_size;

			double segment = round((high - low) / step_size);

			if (segment <= segnumber)
			{
				Ymax = high;
				Ymin = low;
				return TRUE;
			}
		}
		printf("[!] Error at FindGlobalMaxScale()\nimpossible to calculate good proportion with the algorythm\n");
		return FALSE;
		
	}
	printf("[!] Error at FindGlobalMaxScale()\nsignal count  = 0\n");
	return FALSE;
}



/*-------------------------------------------------------------------------
	DrawWave: Compute every signals for display
  -------------------------------------------------------------------------*/

VOID DrawWave(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at DrawWave() graph handle is null\n");
		return;
	}

	if (pgraph->cur_nbpoints > 0)
	{
		DATA* pDATA;
		glLineWidth(1.5);

		for (int index = 0; index < pgraph->signalcount; index++)
		{
			pDATA = pgraph->signal[index];
			if (pDATA->show == false)																	// Skip signal display if requiered
			{
				continue;
			}
			glColor3f(pDATA->color[0], pDATA->color[1], pDATA->color[2]);								// Colors of the signal
			glBegin(GL_LINE_STRIP);

			int first_point = pgraph->cur_nbpoints - (pgraph->cur_nbpoints / GetZoomFactor(hGraph));	// first_point [0-cur_nbpoints]
			if (first_point < 0)
				first_point = 0;

			for (int i = first_point; i < pgraph->cur_nbpoints; i++)	// Handle zoom here
			{
					// prevent NAN

				if (pDATA->Xnorm[i] != pDATA->Xnorm[i] || pDATA->Ynorm[i] != pDATA->Ynorm[i])
					continue;

				glVertex2f(TakeFiniteNumber(pDATA->Xnorm[i]), TakeFiniteNumber(pDATA->Ynorm[i]));		// Create GPU wave
			}
			glEnd();
		}
	}
}

/*-------------------------------------------------------------------------
	DrawString: Display character in OpenGL
  -------------------------------------------------------------------------*/

VOID DrawString(float x, float y, char* string)
{
	glRasterPos2f(x, y);
	glPrint(string);  // Print GL Text To The Screen
}

/*-------------------------------------------------------------------------
	DrawGraphSquare: Make the graph boxing here
  -------------------------------------------------------------------------*/

VOID DrawGraphSquare(VOID)
{
		// Set boxing

	glLineWidth(3);
	glColor3f(0.0f, 0.0f, 0.0f);
	glBegin(GL_LINE_STRIP);
	glVertex2i(0, 0);
	glVertex2i(1, 0);
	glVertex2i(1, 1);
	glVertex2i(0, 1);
	glVertex2i(0, 0);
	glEnd();

		// Set colored font rectangle

	glBegin(GL_POLYGON);
	glColor3f(0.988f, 0.988f, 0.988f);
	glVertex2i(0, 0);

	glColor3f(0.888f, 0.888f, 0.890f);
	glVertex2i(0, 1);

	glColor3f(0.888f, 0.888f, 0.890f);
	glVertex2i(1, 1);

	glColor3f(0.988f, 0.988f, 0.988f);
	glVertex2i(1, 0);

	glEnd();
}

/*-------------------------------------------------------------------------
	DrawGridLines: draw a grid in the square
  -------------------------------------------------------------------------*/

VOID DrawGridLines(VOID)
{
		// Draw fine grid 

	glLineWidth(0.5);
	glColor3f(0.8F, 0.8F, 0.8F);
	glBegin(GL_LINES);
	for (float xtmp = 0.0f; xtmp < 1.0f; xtmp += 0.05f)
	{
		glVertex2f(xtmp, 0.0);
		glVertex2f(xtmp, 1.0);
		glVertex2f(0.0, xtmp);
		glVertex2f(1.0, xtmp);
	};
	glEnd();

		//Draw Grid 

	glLineWidth(1);
	glColor3f(0.5F, 0.5F, 0.5F);
	glBegin(GL_LINES);
	for (float xtmp = 0.0f; xtmp < 1.0f; xtmp += 0.20f)
	{
		glVertex2f(xtmp, 0.0);
		glVertex2f(xtmp, 1.0);
		glVertex2f(0.0, xtmp);
		glVertex2f(1.0, xtmp);
	};
	glEnd();
}

/*-------------------------------------------------------------------------
	DrawCursor: draw a moveable triangle on the graph
  -------------------------------------------------------------------------*/

VOID DrawCursor(float x, float y)
{
	glLineWidth(2.0f);
	glColor3f(0.0f, 0.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	glVertex2f(x - 0.01f, y + 0.01f);
	glVertex2f(x, y);
	glVertex2f(x - 0.01f, y - 0.01f);
	glEnd();
}

/*-------------------------------------------------------------------------
	TakeFiniteNumber: Ensure to return a reel value as the graph can't plot
	+-INF value. A rounding happen here
  -------------------------------------------------------------------------*/

inline double TakeFiniteNumber(double x)
{
	// used to prevent -INF && +INF for computation
	// always return a real value closest to the limit

	if (x <= -DBL_MAX)
	{
		return -DBL_MAX;
	}
	if (x >= +DBL_MAX)
	{
		return DBL_MAX;
	}
	return x;
}

/*-------------------------------------------------------------------------
	FindFirstFiniteNumber: Iterate the array and returning the first reel
	finite number
  -------------------------------------------------------------------------*/

double FindFirstFiniteNumber(double* tab, int length)
{
	int i = 0;
	do
	{
		if (tab[i] == tab[i])
		{
			return tab[i];
		}
		i++;
	} while (i <= length);
	return 0;
}

/*-------------------------------------------------------------------------
	ftos: format a float value to a str representation
  -------------------------------------------------------------------------*/

LPSTR dtos(LPSTR str, int len, double value)
{
	

	if (value < 1.0 && value > -1.0)
	{	
		sprintf_s(str, len, "%.3lf", value);
	}
	else if (value < 10.0 && value > -10.0)
	{
		sprintf_s(str, len, "%.2lf", value);
	}
	else
	{
		sprintf_s(str, len, "%.1lf", value);
	}
	
	return str;
}

/*-------------------------------------------------------------------------
	GetStandardizedData: return the reel value from 
	the normalized data space [0;1]
  -------------------------------------------------------------------------*/

double GetStandardizedData(double X, double min, double max)
{
	double ret;
	ret = (X - min) / (max - min);
	// initial value = (max - X) / (max - min);
	return ret;
}

/*-------------------------------------------------------------------------
	normalize_data: Update the DATA object. The datas will
	be set between the range of 0 and 1 after executing.
  -------------------------------------------------------------------------*/

VOID normalize_data(HGRAPH hGraph, double Xmin, double Xmax, double Ymin, double Ymax)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at normalize_data() graph handle is null\n");
		return;
	}

	for (int index = 0; index < pgraph->signalcount; index++)
	{
		for (int x = 0; x < pgraph->cur_nbpoints; x++)
		{

			// prevent Nan numbers

			if (pgraph->signal[index]->Y[x] != pgraph->signal[index]->Y[x] || Xmax == Xmin)
				continue;

			// Xnorm = (X - min) / (max - min);

			pgraph->signal[index]->Xnorm[x] = (pgraph->signal[index]->X[x] - Xmin) / (Xmax - Xmin);
			pgraph->signal[index]->Ynorm[x] = (pgraph->signal[index]->Y[x] - Ymin) / (Ymax - Ymin);

			// prevent Nan numbers in normalized buffer
			
			////////////////////////////////////////////////////////////////////////////////!
			// Error prone = 0.0 in place of +inf or -inf
			if (pgraph->signal[index]->Ynorm[x] != pgraph->signal[index]->Ynorm[x])
				pgraph->signal[index]->Y[x] = 0.0;


		}
	}
}

/*-------------------------------------------------------------------------
	UpdateBorder: Update the min and max value of every signal object 
	in the struct.
  -------------------------------------------------------------------------*/
VOID UpdateBorder(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

		// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at UpdateBorder() graph handle is null\n");
		return;
	}

	static int AnalizedPts;
	static int CurrentPoint = 0;
	/// ////////////////////////////////////////////
	static int xmaxpos;
	static int yminpos;
	static int ymaxpos;

		// Runonce

	if (0 == runonce)
	{
		AnalizedPts = 0;
		xmaxpos = 0;
		yminpos = 0;
		ymaxpos = 0;
		runonce++;
	}
	//fprintf("\rAnalizedPts%i xmax%i ymin%i ymax%i", AnalizedPts, xmaxpos, yminpos, ymaxpos);
	if (yminpos < 0 || ymaxpos < 0 || yminpos > pgraph->cur_nbpoints/20 || ymaxpos > pgraph->cur_nbpoints/20)
	{
		int zoom = GetZoomFactor(hGraph);
		if (zoom > 0)
		{
			int low = pgraph->cur_nbpoints - (pgraph->cur_nbpoints / zoom);
			//printf("\r\nlower point analysed: %i", low);
			AnalizedPts = low;
		}
		else
			AnalizedPts = 0;

		//AnalizedPts = 0;
		for (int index = 0; index <= pgraph->signalcount - 1; index++)
		{
			if (pgraph->signal[index]->show == false)
			{
				continue;
			}
			pgraph->signal[index]->Xmin = TakeFiniteNumber(pgraph->signal[index]->X[AnalizedPts]);
			pgraph->signal[index]->Xmax = TakeFiniteNumber(pgraph->signal[index]->X[AnalizedPts]);
			//pgraph->BufferSize = 50;
			
			if (pgraph->bAutoscale == TRUE)
			{
				pgraph->signal[index]->Ymin = TakeFiniteNumber(pgraph->signal[index]->Y[AnalizedPts]);
				pgraph->signal[index]->Ymax = TakeFiniteNumber(pgraph->signal[index]->Y[AnalizedPts]);
			}
			else
			{
				pgraph->signal[index]->Ymin = pgraph->ymin_fix;

				pgraph->signal[index]->Ymax = pgraph->ymax_fix;
			}

		}
	}

	/// ////////////////////////////////////////////

	if (0 == pgraph->cur_nbpoints)
	{
		for (int index = 0; index <= pgraph->signalcount - 1; index++)
		{
			pgraph->signal[index]->Xmin = 0.0f;
			pgraph->signal[index]->Xmax = 0.0f;
			pgraph->signal[index]->Ymin = 0.0f;
			pgraph->signal[index]->Ymax = 0.0f;
		}
		return;
	}

	if (1 == pgraph->cur_nbpoints)
	{
		for (int index = 0; index <= pgraph->signalcount - 1; index++)
		{
			if (pgraph->signal[index]->show == false)
			{
				continue;
			}
			pgraph->signal[index]->Xmin = TakeFiniteNumber(pgraph->signal[index]->X[AnalizedPts]);
			pgraph->signal[index]->Xmax = TakeFiniteNumber(pgraph->signal[index]->X[AnalizedPts]);
			pgraph->signal[index]->Ymin = TakeFiniteNumber(pgraph->signal[index]->Y[AnalizedPts]);
			pgraph->signal[index]->Ymax = TakeFiniteNumber(pgraph->signal[index]->Y[AnalizedPts]);
		}
	}

	if (1 < pgraph->cur_nbpoints)
	{
		for (int index = 0; index <= pgraph->signalcount - 1; index++)
		{
			if (pgraph->signal[index]->show == false)
			{
				continue;
			}
			CurrentPoint = AnalizedPts;
		
			for (CurrentPoint; CurrentPoint < pgraph->cur_nbpoints; CurrentPoint++)
			{
				pgraph->signal[index]->Xmin = TakeFiniteNumber(pgraph->signal[index]->X[AnalizedPts]);
				//pgraph->signal[index]->Xmin = TakeFiniteNumber(pgraph->signal[index]->X[0]);

				if (TakeFiniteNumber(pgraph->signal[index]->X[CurrentPoint]) > pgraph->signal[index]->Xmax)
				{
					pgraph->signal[index]->Xmax = TakeFiniteNumber(pgraph->signal[index]->X[CurrentPoint]);
					xmaxpos = CurrentPoint;
				}	

				if (TakeFiniteNumber(pgraph->signal[index]->Y[CurrentPoint]) < pgraph->signal[index]->Ymin)
				{
					pgraph->signal[index]->Ymin = TakeFiniteNumber(pgraph->signal[index]->Y[CurrentPoint]);
					yminpos = CurrentPoint;										
				}
					
				if (TakeFiniteNumber(pgraph->signal[index]->Y[CurrentPoint]) > pgraph->signal[index]->Ymax)
				{
					pgraph->signal[index]->Ymax = TakeFiniteNumber(pgraph->signal[index]->Y[CurrentPoint]); 
					ymaxpos = CurrentPoint;
				}	
			}
		}
	}

	AnalizedPts = pgraph->cur_nbpoints - 1;
	if (pgraph->cur_nbpoints == pgraph->BufferSize)
	{
		AnalizedPts--;
	}	
	/// ////////////////////////////////////////////
	xmaxpos--;
	yminpos--;
	ymaxpos--;
	/// ////////////////////////////////////////////
}

/*-------------------------------------------------------------------------
	ZeroObject: Reset DATA structure to 0
-------------------------------------------------------------------------*/

VOID ZeroObject(DATA *pDATA, INT iBufferSize)
{
		// Sanity check

	if (NULL == pDATA)
	{
		printf("[!] Error at ZeroObject() pData ptr is null\n");
		return;
	}

	memset(pDATA->X, 0, sizeof(double) * iBufferSize);
	memset(pDATA->Y, 0, sizeof(double) * iBufferSize);
	memset(pDATA->Xnorm, 0, sizeof(double) * iBufferSize);
	memset(pDATA->Ynorm, 0, sizeof(double) * iBufferSize);
	pDATA->Xmin = 0.0f;
	pDATA->Xmax = 0.0f;
	pDATA->Ymin = 0.0f;
	pDATA->Ymax = 0.0f;
}

/*-------------------------------------------------------------------------
	GetBufferSize: return the total data buffer size
-------------------------------------------------------------------------*/

INT GetBufferSize(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	// Sanity check

	if (NULL == pgraph)
	{
		printf("[!] Error at GetBufferSize() graph handle is null\n");
		return -1;
	}
	return pgraph->BufferSize;
}

/*-------------------------------------------------------------------------
	GetUniqueFilename: return an unique filename for logging purpose
-------------------------------------------------------------------------*/

BOOL GetUniqueFilename(CHAR* lpFilename, CHAR* lpFileExtension)
{
	SYSTEMTIME t;

		// get daily filename

	char lpDateStr[MAX_PATH] = "";
	char name_format[32] = "";
	if (strlen(lpFileExtension) > 10)
	{
		printf("[!] Error at GetUniqueFilename() lpFileExtension > 10 char\n");
		return FALSE;
	}
	sprintf_s(name_format, "yyyy_MMM_dd_%s", lpFileExtension);
	GetLocalTime(&t);
	int ok = GetDateFormat(LOCALE_USER_DEFAULT,
		0,
		&t,
		name_format,
		lpDateStr,
		sizeof(lpDateStr));

	CHAR lpBaseFilename[MAX_PATH] = "";
	strcpy_s(lpBaseFilename, _countof(lpDateStr), lpDateStr);
	strcpy_s(lpFilename, _countof(lpBaseFilename), lpBaseFilename);

	int i = 0;
try_next_file:
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(
		lpFilename,
		&FindFileData
	);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("[*] Make unique filename successfull: %s\n", lpFilename);
		return TRUE;
	}

	// Build (0) str
	char temp[6];
	snprintf(temp, _countof(temp), "(%i)", i);

	// Reset lpFilename with base file
	strcpy_s(lpFilename, _countof(lpBaseFilename), lpBaseFilename);

	// Remove extension .lab
	int cut_offset = strlen(lpFilename) - strlen(lpFileExtension);
	lpFilename[cut_offset] = '\0';

	// Add (0)
	strcat_s(lpFilename, _countof(lpBaseFilename), temp);

	// Add extension
	strcat_s(lpFilename, _countof(lpBaseFilename), lpFileExtension);

	// Inc counter
	i++;

	// Max filenumber = 999
	if(i<999)
		goto try_next_file;


	printf("[!] Make unique filename failed\n");
	return FALSE;
}

inline long long PerformanceFrequency()
{
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return li.QuadPart;
}

inline long long PerformanceCounter()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

/*-------------------------------------------------------------------------
	CheckErr: try to catch openGL error messages
  -------------------------------------------------------------------------*/

VOID CheckErr(VOID)
{
	GLenum err = 0;
	err = glGetError();
	switch (err)
	{
	case GL_NO_ERROR:
		printf("	[!] GL_NO_ERROR\n");
		break;

	case GL_INVALID_ENUM:
		printf("	[!] GL_INVALID_ENUM");
		break;

	case GL_INVALID_VALUE:
		printf("	[!] GL_INVALID_VALUE");
		break;

	case GL_INVALID_OPERATION:
		printf("	[!] GL_INVALID_OPERATION");
		break;

	case GL_STACK_OVERFLOW:
		printf("	[!] GL_STACK_OVERFLOW");
		break;

	case GL_STACK_UNDERFLOW:
		printf("	[!] GL_STACK_UNDERFLOW");
		break;

	case GL_OUT_OF_MEMORY:
		printf("	[!] GL_OUT_OF_MEMORY");
		break;

	default:
		break;
	}
}
