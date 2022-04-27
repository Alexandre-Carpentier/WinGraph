#include "graph.h"
// Private declarations goes here
#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "User32.lib")
#pragma comment (lib, "Opengl32.lib")
#pragma comment (lib, "Glu32.lib")

enum { MAX_SIGNAL_COUNT = 16 };

VOID InitGL(HGRAPH hGraph, int Width, int Height);
VOID SetGLView(int Width, int Height);
VOID CheckErr(VOID);

BOOL BuildMyFont(HGRAPH hGraph, char* FontName, int Fontsize);
void KillFont(GLvoid);
GLvoid glPrint(const char* fmt, ...);

BOOL FindGlobalMaxScale(HGRAPH hGraph, float& Xmin, float& Xmax, float& Ymin, float& Ymax);
VOID DrawWave(HGRAPH hGraph);
VOID DrawString(float x, float y, char* string);
VOID DrawGraphSquare(VOID);
VOID DrawGridLines(VOID);
VOID DrawCursor(float x, float y);


inline float TakeFiniteNumber(float x);
float FindFirstFiniteNumber(float* tab, int length);
LPSTR ftos(LPSTR str, int len, float value);
float GetStandardizedData(float X, float min, float max);
VOID normalize_data(HGRAPH hGraph, float Xmin, float Xmax, float Ymin, float Ymax);
VOID UpdateBorder(HGRAPH hGraph);


inline long long PerformanceFrequency();
inline long long PerformanceCounter();

typedef struct {
	float *X;
	float *Y;
	float *Xnorm;
	float *Ynorm;
	float Xmin;
	float Xmax;
	float Ymin;
	float Ymax;
}DATA;

VOID ZeroObject(HGRAPH hGraph, DATA* pDATA);
#pragma warning(disable : 4200)					// Disable warning: DATA* signal[] -> Array size [0], See CreateGraph for signal allocation specifics
typedef struct {
	HWND hParentWnd;							// Parent handle of the object
	HWND hGraphWnd;								// Graph handle
	HDC hDC;									// OpenGL device context
	HGLRC hRC;									// OpenGL rendering context
	INT signalcount;							// Total signals in the struct
	INT cur_nbpoints;							// Current total points in the arrays
	INT BufferSize;								// The total amount of point to handle							
	BOOL bRunning;								// Status of the graph
	BOOL bLogging;								// Logging active
	DATA* signal[];								// ! (flexible array member) Array of pointers for every signal to be store by the struct - Must be last member of the struct
}GRAPHSTRUCT, * PGRAPHSTRUCT;					// Declaration of the struct. To be cast from HGRAPH api

// Global access

DATA* SnapPlot;									// SnapPlot: work with temp data on signals[], used to convert standard values to normalized values
RECT DispArea;									// RECT struct for the OpenGL area dimensions stored in WinProc
GLuint  base;                                   // Base Display List For The Font Set
SIZE dispStringWidth;							// The size  in pixel of "-0.000" displayed on screen
CRITICAL_SECTION cs;							// Sync purpose
FILE* logfile;									// The log file
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
	PFD_DOUBLEBUFFER,							// Must Support Double Buffering
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
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	DATA* pDATA;

	// Sanity check

	if (NULL == pgraph || TRUE == pgraph->bRunning)
		return FALSE;

	// reset counters and data array of signals

	pgraph->cur_nbpoints = 0;
	for (int index = 0; index < pgraph->signalcount; index++)
	{
		pDATA = pgraph->signal[index];
		memset(pDATA->X, 0, sizeof(float)* pgraph->BufferSize);
		memset(pDATA->Y, 0, sizeof(float) * pgraph->BufferSize);
		memset(pDATA->Xnorm, 0, sizeof(float) * pgraph->BufferSize);
		memset(pDATA->Ynorm, 0, sizeof(float) * pgraph->BufferSize);
		pDATA->Xmin = 0.0f;
		pDATA->Xmax = 0.0f;
		pDATA->Ymin = 0.0f;
		pDATA->Ymax = 0.0f;
	}

	// Save the start time x=0

	frequency = PerformanceFrequency();
	start = PerformanceCounter();

	// Create the log file
	if (pgraph->bLogging == TRUE)
	{
		logfile = NULL;
		char szFilename[MAX_PATH] = "Log.txt";
		fopen_s(&logfile, szFilename, "w");
		if (logfile)
		{
			fprintf(logfile, "Time(s)\tForce (N)\n");					// make logfile header
		}
		else
		{
			return FALSE;
		}
	}

	// Update status -> Graph ON

	pgraph->bRunning = TRUE;


	// reset runonce flag
	runonce = 0;

	return TRUE;
}

/*-------------------------------------------------------------------------
	StopGraph: Close the logfile if needed and 
	update state flags
  -------------------------------------------------------------------------*/

VOID StopGraph(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	if (NULL == pgraph)
		return;

	//Close the log file properly

	if (pgraph->bLogging == TRUE)
	{
		if (logfile)
		{
			fclose(logfile);
			logfile = NULL;
		}
	}

	// Update status -> Graph OFF

	pgraph->bRunning = FALSE;
	pgraph->cur_nbpoints = 0;

}

/*-------------------------------------------------------------------------
	FreeGraph: Free every buffer allocated by malloc
	Realease the device context and delete the object
  -------------------------------------------------------------------------*/

VOID FreeGraph(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	if (pgraph)
	{

		DATA* pDATA;
		for (int index = 0; index < pgraph->signalcount; index++)
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

		pgraph = NULL;
		DeleteCriticalSection(&cs);

	}
}



/*-------------------------------------------------------------------------
	CreateGraph: Initialize the structure, signals,
	OpenGL and critical section. return HGRAPH
  -------------------------------------------------------------------------*/

HGRAPH CreateGraph(HWND hWnd, RECT GraphArea, INT SignalCount, INT BufferSize )
{
	int PFDID;
	GRAPHSTRUCT* pgraph = NULL;

		// Sanity check

	if (NULL == hWnd)
		return NULL;

	if (0 == SignalCount || MAX_SIGNAL_COUNT < SignalCount || 0 >= BufferSize)
		return NULL;

		// Initialyze sync

	InitializeCriticalSection(&cs);

		// Init struct

	if (NULL == (pgraph = (GRAPHSTRUCT*)malloc(sizeof(GRAPHSTRUCT) + sizeof(void*) * SignalCount)))		// Carefully taking in account signal declaration DATA*signal[], so allocate space for each new ptr on the fly
		return NULL;	// Otherwize Heap will be corrupted

		// Struct memory zero at startup

	memset(pgraph, 0, sizeof(GRAPHSTRUCT) + sizeof(void*) * SignalCount);

		// Allocate each signal buffers on the Heap and fill with zero

	for (int i = 0; i < SignalCount; i++)
	{
		if (NULL == (pgraph->signal[i] = (DATA*)malloc(sizeof(DATA))))
			return NULL;
		DATA* pDATA = pgraph->signal[i];

		if (NULL == (pDATA->X = (float*)malloc(sizeof(float) * BufferSize)))
			return NULL;		

		if (NULL == (pDATA->Y = (float*)malloc(sizeof(float) * BufferSize)))
			return NULL;	

		if (NULL == (pDATA->Xnorm = (float*)malloc(sizeof(float) * BufferSize)))
			return NULL;

		if (NULL == (pDATA->Ynorm = (float*)malloc(sizeof(float) * BufferSize)))
			return NULL;

		memset(pDATA->X, 0, sizeof(float) * BufferSize);
		memset(pDATA->Y, 0, sizeof(float) * BufferSize);
		memset(pDATA->Xnorm, 0, sizeof(float) * BufferSize);
		memset(pDATA->Ynorm, 0, sizeof(float) * BufferSize);

		pDATA->Xmax = 0.0;
		pDATA->Xmin = 0.0;
		pDATA->Ymax = 0.0;
		pDATA->Ymin = 0.0;

			// Update the number of signal inside object HGRAPH

		pgraph->signalcount++;
	}

	pgraph->BufferSize = BufferSize;

		// Allocate a temp struct for computing

	SnapPlot = NULL;
	if(NULL== (SnapPlot = (DATA*)malloc(sizeof(DATA))))
		return NULL;

	if (NULL == (SnapPlot->X = (float*)malloc(sizeof(float) * BufferSize)))
		return NULL;

	if (NULL == (SnapPlot->Y = (float*)malloc(sizeof(float) * BufferSize)))
		return NULL;

	if (NULL == (SnapPlot->Xnorm = (float*)malloc(sizeof(float) * BufferSize)))
		return NULL;

	if (NULL == (SnapPlot->Ynorm = (float*)malloc(sizeof(float) * BufferSize)))
		return NULL;

	memset(SnapPlot->X, 0, sizeof(float) * BufferSize);
	memset(SnapPlot->Y, 0, sizeof(float) * BufferSize);
	memset(SnapPlot->Xnorm, 0, sizeof(float) * BufferSize);
	memset(SnapPlot->Ynorm, 0, sizeof(float) * BufferSize);

	pgraph->hParentWnd = hWnd;

	// Graph created in a "Static" control windows class named "Graph"
	// When redrawn the control will be painted with the graph in place of

	pgraph->hGraphWnd = CreateWindow(
		"Static",													// Predefined class; Unicode assumed 
		"Graph",													// The text will be erased by OpenGL
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
		return NULL;

	pgraph->hDC = GetDC(pgraph->hGraphWnd);

	// Sanity check

	if (NULL == pgraph->hDC)
		return NULL;

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
		MessageBox(0, "Can't Find A Suitable PixelFormat.", "Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(0);
		return NULL;
	}

	if (SetPixelFormat(pgraph->hDC, PFDID, &pfd) == false)
	{
		MessageBox(0, "Can't Set The PixelFormat.", "Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(0);
		return NULL;
	}

	// Rendering Context

	pgraph->hRC = wglCreateContext(pgraph->hDC);

	if (pgraph->hRC == 0)
	{
		MessageBox(0, "Can't Create A GL Rendering Context.", "Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(0);
		return NULL;
	}

	if (wglMakeCurrent(pgraph->hDC, pgraph->hRC) == false)
	{
		MessageBox(0, "Can't activate GLRC.", "Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(0);
		return NULL;
	}
	pgraph->bLogging = false;
	GetClientRect(pgraph->hGraphWnd, &DispArea);
	InitGL(pgraph, DispArea.right, DispArea.bottom);
	return pgraph;
}

/*-------------------------------------------------------------------------
	SetRecordingMode: set the graph reccording state with bLogging
  -------------------------------------------------------------------------*/

VOID SetRecordingMode(HGRAPH hGraph, BOOL logging)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	if (NULL == pgraph)
		return;

	pgraph->bLogging = logging;
}

/*-------------------------------------------------------------------------
	GetGraphRC: return the graph state from bRunning
  -------------------------------------------------------------------------*/

BOOL GetGraphState(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = NULL;
	pgraph = (PGRAPHSTRUCT)hGraph;
	if (NULL == pgraph)
		return FALSE;

	return pgraph->bRunning;

}

/*-------------------------------------------------------------------------
	GetGraphRC: return the graph render context
  -------------------------------------------------------------------------*/

HGLRC GetGraphRC(HGRAPH hGraph)
{
	// Sanity check

	if (NULL == hGraph)
		return NULL;

	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	if (NULL == pgraph->hRC)
		return NULL;

	return pgraph->hRC;
}

/*-------------------------------------------------------------------------
	GetGraphDC: return the graph device context
  -------------------------------------------------------------------------*/

HDC GetGraphDC(HGRAPH hGraph)
{
	// Sanity check

	if (NULL == hGraph)
		return NULL;

	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	if (NULL == pgraph->hDC)
		return NULL;

	return pgraph->hDC;
}

/*-------------------------------------------------------------------------
	GetGraphParentWnd: return the graph parent HWND
  -------------------------------------------------------------------------*/

HWND GetGraphParentWnd(HGRAPH hGraph)
{
	// Sanity check

	if (NULL == hGraph)
		return NULL;

	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	if (NULL == pgraph->hParentWnd)
		return NULL;

	return pgraph->hParentWnd;
}

/*-------------------------------------------------------------------------
	GetGraphWnd: return the graph HWND
  -------------------------------------------------------------------------*/

HWND GetGraphWnd(HGRAPH hGraph)
{
	// Sanity check

	if (NULL == hGraph)
		return NULL;

	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	if (NULL == pgraph->hGraphWnd)
		return NULL;

	return pgraph->hGraphWnd;
}

/*-------------------------------------------------------------------------
	GetGraphSignalNumber: return the total signals number
  -------------------------------------------------------------------------*/

INT GetGraphSignalCount(HGRAPH hGraph)
{
	// Sanity check

	if (NULL == hGraph)
		return NULL;

	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	if (NULL == pgraph->signalcount)
		return NULL;

	return pgraph->signalcount;
}


VOID AddPoints(HGRAPH hGraph, float* y, INT PointsCount)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	DATA* pDATA = NULL;
	if (cs.DebugInfo == NULL)
	{
		return;
	}

	EnterCriticalSection(&cs);

	// Sanity check

	if (NULL == pgraph || FALSE == pgraph->bRunning)
	{
		LeaveCriticalSection(&cs);
		return;
	}

	// TODO: Check if signalcount = length of y!

	if (pgraph->signalcount != PointsCount)
	{
		LeaveCriticalSection(&cs);
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

	if (logfile)
	{
		fprintf(logfile, "%lf\t", (float)((finish - start)) / frequency);
	}
	for (int index = 0; index < pgraph->signalcount; index++)
	{
		pDATA = pgraph->signal[index];
		if (NULL == pDATA)
		{
			LeaveCriticalSection(&cs);
			return;
		}

		// Add points to the selected buffer	

		pDATA->X[pgraph->cur_nbpoints] = (float)((finish - start)) / frequency;								// Save in X the elapsed time from start
		pDATA->Y[pgraph->cur_nbpoints] = y[index];															// Save Y
		if (logfile)
		{
			fprintf(logfile, "%lf\t", y[index]);
		}
	}
	if (logfile)
	{
		fprintf(logfile, "\n");
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
	// Sanity check

	if (NULL == hGraph)
		return FALSE;

	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	if (NULL == pgraph->hGraphWnd)
		return FALSE;

	if (pgraph->cur_nbpoints > 0)
	{
		EnterCriticalSection(&cs);

		//long long t1 = PerformanceCounter(); //DBG

		UpdateBorder(hGraph);																			// border determination for each signal: meaning finding X and Y min max values																		
		//DATA** sig = pgraph->signal; //DBG

		//long long t2 = PerformanceCounter(); //DBG
		//double freq = (double)((t2 - t1)) / frequency * 1000; //DBG
		//printf("\rupdate_border take: %lf ms\r", freq); //DBG

		//memset(SnapPlot, 0, sizeof(DATA));															// Reset computing datas
		ZeroObject(hGraph, SnapPlot);
		FindGlobalMaxScale(hGraph, SnapPlot->Xmin, SnapPlot->Xmax, SnapPlot->Ymin, SnapPlot->Ymax);		// Finding the Y min and max of all the signals to scale on ite
		normalize_data(hGraph, SnapPlot->Xmin, SnapPlot->Xmax, SnapPlot->Ymin, SnapPlot->Ymax);			// normalize between [0;1]
		GetClientRect(pgraph->hGraphWnd, &DispArea);

		// Use the Projection Matrix

		glMatrixMode(GL_PROJECTION);

		// Reset Matrix

		glLoadIdentity();

		// Set the correct perspective.

		gluOrtho2D(-0.08, 1.04, 0 - 0.08, 1 + 0.02);
		glViewport(0, 0, DispArea.right, DispArea.bottom);

		// Use the Model Matrix

		glMatrixMode(GL_MODELVIEW);

		// Reset Matrix

		glLoadIdentity();

		// Clear Color and Depth Buffers

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// to run once

		glClear(GL_COLOR_BUFFER_BIT);

		DrawGraphSquare();
		DrawGridLines();

		if (SnapPlot->Ymax != SnapPlot->Ymin)
		{
			float txtlen = 0.0;
			float txtheight = 0.0;
			char value[64];
			char Xname[] = "Time (s)";
			float reelval = SnapPlot->Ymin;
			int div = 5;

			// draw points

			DrawWave(hGraph);

			// draw indicators

			glColor3f(0.1f, 0.1f, 0.1f);

			// zero

			float zero = GetStandardizedData(0, SnapPlot->Ymin, SnapPlot->Ymax);
			if (zero > 0 && zero < 1)
			{
				DrawString(-0.02f, zero - 0.01f, (char*)"0");
				DrawCursor(0.0f, zero);
			}

			// Determine the length of a typical string for resizing purpose

			RECT r;
			GetWindowRect(GetGraphWnd(hGraph), &r);
			txtlen = (float)dispStringWidth.cx / r.right; // Normalize the width of the Y value characters between [0-1]
			txtheight = (float)dispStringWidth.cy / r.bottom;

			//Xmin

			DrawString(-txtlen / 1.2f, -0.05f, ftos(value, sizeof(value), SnapPlot->Xmin));

			//Xmax

			DrawString(1 - txtlen / 1.2f, -0.05f, ftos(value, sizeof(value), SnapPlot->Xmax));

			//Time (s)

			DrawString(0.5f - (txtlen / 2.0f), -0.05f, Xname);

			// Ymin to Ymax values

			for (float ytmp = 0.0f; ytmp <= 1.0f; ytmp += 1.0f / div)
			{
				DrawString(-txtlen * 1.8f, ytmp - ((txtheight * 0.8f) / 2.0f), ftos(value, sizeof(value), reelval));
				reelval += (SnapPlot->Ymax - SnapPlot->Ymin) / div;
			}
		}
		LeaveCriticalSection(&cs);
		SwapBuffers(GetGraphDC(hGraph));
	}
	else if (pgraph->cur_nbpoints == 0 && start == 0.0f)														// Display a void graph when app start only
	{
		char value[32];
		char Xname[] = "Time (s)";
		float txtlen = 0.0;
		float txtheight = 0.0;
		RECT r;
		SnapPlot->Xmin = 0.0f;
		SnapPlot->Xmax = 1.0f;
		SnapPlot->Ymin = 0.0f;
		SnapPlot->Ymax = 1.0f;
		GetClientRect(pgraph->hGraphWnd, &DispArea);

			// Use the Projection Matrix

		glMatrixMode(GL_PROJECTION);

			// Reset Matrix

		glLoadIdentity();

			// Set the correct perspective.

		gluOrtho2D(-0.08, 1.04, 0 - 0.08, 1 + 0.02);
		glViewport(0, 0, DispArea.right, DispArea.bottom);

			// Use the Projection Matrix

		glMatrixMode(GL_MODELVIEW);

			// Reset Matrix

		glLoadIdentity();

			// Clear Color and Depth Buffers

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// to run once

		glClear(GL_COLOR_BUFFER_BIT);
		DrawGraphSquare();
		DrawGridLines();

			// draw indicators

		glColor3f(0.1f, 0.1f, 0.1f);

			// zero

		float zero = GetStandardizedData(0, SnapPlot->Ymin, SnapPlot->Ymax);
		if (zero > 0 && zero < 1)
		{
			//DrawString(-0.05, zero, (char*)"0");
			DrawCursor(0.0f, zero);
		}

			// Determine the length of a typical string for resizing purpose

		GetWindowRect(GetGraphWnd(hGraph), &r);
		txtlen = (float)dispStringWidth.cx / r.right; // Normalize the width of the Y value characters between [0-1]
		txtheight = (float)dispStringWidth.cy / r.bottom;

			//Xmin

		DrawString(-txtlen / 1.2f, -0.05f, ftos(value, sizeof(value), SnapPlot->Xmin));

			//Xmax

		DrawString(1 - txtlen / 1.2f, -0.05f, ftos(value, sizeof(value), SnapPlot->Xmax));

			//Time (s)

		DrawString(0.5f - (txtlen / 2.0f), -0.05f, Xname);

			// Ymin to Ymax values

		float reelval = SnapPlot->Ymin;
		int div = 5;
		for (float ytmp = 0.0f; ytmp <= 1.0f; ytmp += 1.0f / div)
		{
			DrawString(-txtlen * 1.8f, ytmp - ((txtheight * 0.8f) / 2.0f), ftos(value, sizeof(value), reelval));
			reelval += (SnapPlot->Ymax - SnapPlot->Ymin) / div;
		}
		SwapBuffers(GetGraphDC(hGraph));
	}
	return TRUE;
}

/*-------------------------------------------------------------------------
	ReshapeGraph: When resize message is proceed update graph pos
  -------------------------------------------------------------------------*/

VOID ReshapeGraph(HGRAPH hGraph, int left, int top, int right, int bottom)
{
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
	glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	if (!BuildMyFont(hGraph, (char*)"Verdana", 12))// Build The Font BuildMyFont(HGRAPH hGraph, char* FontName, int Fontsize)
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

	// Create an empty display list of 96 char (We are using ASCII char from 32 to 127 only)

	base = glGenLists(96);	
	if (0 == base)
	{
		// error when generate the empty list
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
		return FALSE;
	}

	hCurrentFont = (HFONT)SelectObject(GetGraphDC(hGraph), hCustomFont);// Select the custom Font and store the current font

	if (!wglUseFontBitmaps(GetGraphDC(hGraph), 32, 96, base))		// Builds 96 Characters Starting At Character 32 and store it in the list
	{
		// error when loading the font
		return FALSE;
	}
	char text[] = "-0.0000";
	SetTextCharacterExtra(GetGraphDC(hGraph), 1);
	GetTextExtentPoint32A(GetGraphDC(hGraph), text, strlen(text), &dispStringWidth);
	dispStringWidth.cx -= GetTextCharacterExtra(GetGraphDC(hGraph)) * (strlen(text) - 2);

	SelectObject(GetGraphDC(hGraph), hCurrentFont);				// restore the initial Font
	DeleteObject(hCustomFont);									// We don't need the Custom Font anymore as we populate the list and load it in OpenGL
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

BOOL FindGlobalMaxScale(HGRAPH hGraph, float& Xmin, float& Xmax, float& Ymin, float& Ymax)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	if (0 != pgraph->signalcount)
	{
		
		Xmin = pgraph->signal[0]->Xmin;										// save first value
		Xmax = pgraph->signal[0]->Xmax;										// save first value	
		Ymin = pgraph->signal[0]->Ymin;										// save first value
		Ymax = pgraph->signal[0]->Ymax;										// save first value


		for (int index = 1; index < pgraph->signalcount; index++)			// Iterate
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
		return TRUE;
	}
	return FALSE;
}



/*-------------------------------------------------------------------------
	DrawWave: Compute every signals for display
  -------------------------------------------------------------------------*/

VOID DrawWave(HGRAPH hGraph)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
	DATA* pDATA;
	glLineWidth(2);

	// Color map

	float COLORS[16][3] =
	{
		{0.1f, 0.15f, 0.15f},
		{0.5f, 0.15f, 0.15f},
		{1.0f, 0.15f, 0.15f},
		{0.1f, 0.5f, 0.15f},
		{0.1f, 0.5f, 0.9f},
		{0.9f, 0.5f, 0.15f},
		{0.1f, 0.15f, 0.5f},
		{0.1f, 0.9f, 0.15f},
		{0.9f, 0.15f, 0.15f},
		{0.1f, 0.15f, 0.15f},
		{0.1f, 0.4f, 0.15f},
		{0.1f, 0.15f, 0.15f},
		{0.1f, 0.7f, 0.15f},
		{0.1f, 0.15f, 0.15f},
		{0.8f, 0.15f, 0.15f},
		{0.1f, 0.8f, 0.8f}
	};

	for (int index = 0; index < pgraph->signalcount; index++)
	{
		pDATA = pgraph->signal[index];
		glColor3f(COLORS[index][0], COLORS[index][1], COLORS[index][2]); // Colors of the signal
		glBegin(GL_LINE_STRIP);
		for (int i = 0; i < pgraph->cur_nbpoints; i++)
		{
			// prevent NAN
			if (pDATA->Xnorm[i] != pDATA->Xnorm[i] || pDATA->Ynorm[i] != pDATA->Ynorm[i])
				continue;

			glVertex2f(TakeFiniteNumber(pDATA->Xnorm[i]), TakeFiniteNumber(pDATA->Ynorm[i])); // Create the curve in memory
		}
		glEnd();
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

	glColor3f(0.988f, 0.99f, 1.0f);
	glBegin(GL_POLYGON);
	glVertex2i(0, 0);
	glVertex2i(0, 1);
	glVertex2i(1, 1);
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
	glColor3f(0.5F, 0.5F, 0.5F);
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
	glColor3f(0.3F, 0.1F, 0.0F);
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

inline float TakeFiniteNumber(float x)
{
	// used to prevent -INF && +INF for computation
	// always return a real value closest to the limit

	if (x <= -FLT_MAX)
	{
		return -FLT_MAX;
	}
	if (x >= +FLT_MAX)
	{
		return FLT_MAX;
	}
	return x;
}

/*-------------------------------------------------------------------------
	FindFirstFiniteNumber: Iterate the array and returning the first reel
	finite number
  -------------------------------------------------------------------------*/

float FindFirstFiniteNumber(float* tab, int length)
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

LPSTR ftos(LPSTR str, int len, float value)
{
	sprintf_s(str, len, "%.1lf", value);
	return str;
}

/*-------------------------------------------------------------------------
	GetStandardizedData: return the reel value from 
	the normalized data space [0;1]
  -------------------------------------------------------------------------*/

float GetStandardizedData(float X, float min, float max)
{
	float ret;
	ret = (X - min) / (max - min);
	// initial value = (max - X) / (max - min);
	return ret;
}

/*-------------------------------------------------------------------------
	normalize_data: Update the DATA object. The datas will
	be set between the range of 0 and 1 after executing.
  -------------------------------------------------------------------------*/

VOID normalize_data(HGRAPH hGraph, float Xmin, float Xmax, float Ymin, float Ymax)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;
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
			// Error prone = 0.0

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
	printf("\rAnalizedPts%i xmax%i ymin%i ymax%i", AnalizedPts, xmaxpos, yminpos, ymaxpos);
	if (yminpos < 0 || ymaxpos < 0 || yminpos > pgraph->cur_nbpoints/10 || ymaxpos > pgraph->cur_nbpoints/10)
	{
		AnalizedPts = 0;
		for (int index = 0; index <= pgraph->signalcount - 1; index++)
		{
			pgraph->signal[index]->Xmin = TakeFiniteNumber(pgraph->signal[index]->X[AnalizedPts]);
			pgraph->signal[index]->Xmax = TakeFiniteNumber(pgraph->signal[index]->X[AnalizedPts]);
			pgraph->signal[index]->Ymin = TakeFiniteNumber(pgraph->signal[index]->Y[AnalizedPts]);
			pgraph->signal[index]->Ymax = TakeFiniteNumber(pgraph->signal[index]->Y[AnalizedPts]);
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
		
			CurrentPoint = AnalizedPts;
		
			for (CurrentPoint; CurrentPoint < pgraph->cur_nbpoints; CurrentPoint++)
			{
				pgraph->signal[index]->Xmin = TakeFiniteNumber(pgraph->signal[index]->X[0]);


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

VOID ZeroObject(HGRAPH hGraph, DATA *pDATA)
{
	PGRAPHSTRUCT pgraph = (PGRAPHSTRUCT)hGraph;

	memset(pDATA->X, 0, sizeof(float) * pgraph->BufferSize);
	memset(pDATA->Y, 0, sizeof(float) * pgraph->BufferSize);
	memset(pDATA->Xnorm, 0, sizeof(float) * pgraph->BufferSize);
	memset(pDATA->Ynorm, 0, sizeof(float) * pgraph->BufferSize);
	pDATA->Xmin = 0.0f;
	pDATA->Xmax = 0.0f;
	pDATA->Ymin = 0.0f;
	pDATA->Ymax = 0.0f;
}

//https://www.pluralsight.com/blog/software-development/how-to-measure-execution-time-intervals-in-c--
inline long long PerformanceFrequency()
{
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return li.QuadPart;
}
//https://www.pluralsight.com/blog/software-development/how-to-measure-execution-time-intervals-in-c--
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