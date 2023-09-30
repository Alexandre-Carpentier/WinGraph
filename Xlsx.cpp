#include "Xlsx.h"

Excel::_WorksheetPtr pSheet;
Excel::_ApplicationPtr xl = nullptr;

HEXCEL excel_create_instance()
{
	try {
		//Initialise the COM interface
		HRESULT hres = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); 
		//HRESULT hres = CoInitialize(NULL);
		if (FAILED(hres))
		{
			MessageBox(GetFocus(), "[!] Failed to Initialize Excel application.", "Error", S_OK | MB_ICONERROR);
			return nullptr;
		}
		//Define a pointer to the Excel application
		xl = nullptr;
		//Start one instance of Excel
		xl.CreateInstance(L"Excel.Application");
		if (xl == nullptr)
		{
			MessageBox(GetFocus(), "[!] Failed to create Excel instance.", "Error", S_OK | MB_ICONERROR);
			return nullptr;
		}

		//Set Excel visible
		xl->PutVisible(0, VARIANT_TRUE);

		//Add a (new) workbook
		xl->Workbooks->Add(Excel::xlWorksheet);


		EXCELSTRUCT* ptr = nullptr;
		ptr = new EXCELSTRUCT;
		if (ptr)
		{
			ptr->inst = xl;
			ptr->line_nb = 0;

			//Get a pointer to the active worksheet
			pSheet = xl->ActiveSheet;
			//Set the name of the sheet
			pSheet->Name = "Chart Data";

			return ptr;
		}
		return nullptr;
	}
	catch (...)
	{
		std::cout << "[!] Error at excel_create_instance().\n";
		return nullptr;
	}
}

bool excel_save(HEXCEL hExcel, const char* filepath)
{
	try {
		EXCELSTRUCT* pData = (EXCELSTRUCT*)hExcel;
		if (pData->inst == nullptr)
		{
			MessageBox(GetFocus(), "[!] excel_save() failed inst is null.", "Error", S_OK | MB_ICONERROR);
			return false;
		}

		xl->Save("File.xlsx", 0);
		return true;
	}
	catch (...)
	{
		std::cout << "[!] Error at excel_save().\n";
		return false;
	}
}

bool excel_addline(HEXCEL hExcel,char lpszLineTab[260])
{
	
	try {
		EXCELSTRUCT* pData = (EXCELSTRUCT*)hExcel;
		if (pData->inst == nullptr)
		{
			MessageBox(GetFocus(), "[!] excel_save() failed inst is null.", "Error", S_OK | MB_ICONERROR);
			return false;
		}

		// Split item from the line separated by \t
		//std::vector<std::string> token;
		std::string line = lpszLineTab;
		std::string element;
		std::stringstream ss(line);
		size_t index = 0;

		pData->line_nb++; // Add new line

		while (std::getline(ss, element, '\t'))
		{
			std::cout << static_cast<void*>(xl);
			std::cout << " -> ";
			std::cout << static_cast<void*>(pSheet);
			std::cout << "\n";

			std::cout << "[*] Inserting: " << element << "\n";
			std::cout << "[*] item[" << pData->line_nb << "][" << index + 1 << "] = " << element << "\n";

			
			//CoMarshalInterThreadInterfaceInStream()
			//CoGetInterfaceAndReleaseStream()

			pSheet->Cells->Item[pData->line_nb][index + 1] = "Test";//element.c_str();

			index++;
		}
		//token.clear();

		std::cout << "\r[*] Excel lines written: " << pData->line_nb << "\n";
		return true;
	}
	catch (...)
	{
		std::cout << "[!] Error at excel_addline().\n";
		return false;
	}
}

bool excel_additem(HEXCEL hExcel, char item[260], int x, int y)
{
	try {
		EXCELSTRUCT* pData = (EXCELSTRUCT*)hExcel;
		if (pData->inst == nullptr)
		{
			MessageBox(GetFocus(), "[!] excel_save() failed inst is null.", "Error", S_OK | MB_ICONERROR);
			return false;
		}

		pSheet->Cells->Item[y][x] = item;

		if(y>pData->line_nb)
			pData->line_nb = y;

		return true;
	}
	catch (...)
	{
		std::cout << "[!] Error at excel_additem().\n";
		return false;
	}
}

bool excel_additem_on_current_line(HEXCEL hExcel, char item[260], int x)
{
	try {
		EXCELSTRUCT* pData = (EXCELSTRUCT*)hExcel;
		if (pData->inst == nullptr)
		{
			MessageBox(GetFocus(), "[!] excel_save() failed inst is null.", "Error", S_OK | MB_ICONERROR);
			return false;
		}

		pSheet->Cells->Item[pData->line_nb][x] = item;

		return true;
	}
	catch (...)
	{
		std::cout << "[!] Error at excel_additem_on_current_line().\n";
		return false;
	}
}

bool excel_cariage_return(HEXCEL hExcel)
{
	try {
		EXCELSTRUCT* pData = (EXCELSTRUCT*)hExcel;
		if (pData->inst == nullptr)
		{
			MessageBox(GetFocus(), "[!] excel_save() failed inst is null.", "Error", S_OK | MB_ICONERROR);
			return false;
		}

		pData->line_nb++;

		return true;
	}
	catch (...)
	{
		std::cout << "[!] Error at excel_cariage_return().\n";
		return false;
	}
}

bool excel_drawgraph(HEXCEL hExcel)
{
	try {
		EXCELSTRUCT* pData = (EXCELSTRUCT*)hExcel;
		if (pData->inst == nullptr)
		{
			MessageBox(GetFocus(), "[!] excel_save() failed inst is null.", "Error", S_OK | MB_ICONERROR);
			return false;
		}

			// Build chart

		//The sheet "Chart Data" now contains all the data
		//required to generate the chart
		//In order to use the Excel Chart Wizard,
		//we must convert the data into Range Objects
		//Set a pointer to the first cell containing our data
		Excel::RangePtr pBeginRange = pSheet->Cells->Item[1][1];
		//Set a pointer to the last cell containing our data
		Excel::RangePtr pEndRange = pSheet->Cells->Item[pData->line_nb][32];
		//Make a "composite" range of the pointers to the start
		//and end of our data
		//Note the casts to pointers to Excel Ranges
		Excel::RangePtr pTotalRange =
			pSheet->Range[(Excel::Range*)pBeginRange][(Excel::Range*)pEndRange];

		// Create the chart as a separate chart item in the workbook
		Excel::_ChartPtr pChart = xl->ActiveWorkbook->Charts->Add();
		//Use the ChartWizard to draw the chart.
		//The arguments to the chart wizard are

		pChart->ChartWizard(
			(Excel::Range*)pTotalRange,//Source: the data range,
			(long)Excel::xlXYScatter,//Gallery: the chart type,
			6L, //Format: a chart format (number 1-10),
			(long)Excel::xlColumns, //PlotBy: whether the data is stored in columns or rows,
			1L, //CategoryLabels: an index for the number of columns containing category (x) labels (because our first column of data represents the x values, we must set this value to 1)
			1L, // 	//SeriesLabels: an index for the number of rows containing series (y) labels (our first row contains y labels, so we set this to 1)
			true,//HasLegend: boolean set to true to include a legend
			"Measurement", //Title: the title of the chart
			"Time(s)", //CategoryTitle: the x-axis title
			"Analog0"//ValueTitle: the y-axis title
		);

		//Give the chart sheet a name
		pChart->Name = "Graph";
		return true;
	}
	catch (...)
	{
		std::cout << "[!] Error at excel_draw_graph().\n";
		return false;
	}
}

bool excel_close(HEXCEL hExcel)
{
	try {
		EXCELSTRUCT* pData = (EXCELSTRUCT*)hExcel;
		if (pData->inst == nullptr)
		{
			MessageBox(GetFocus(), "[!] excel_close() failed inst is null.", "Error", S_OK | MB_ICONERROR);
			return false;
		}

		xl->Quit();
		CoUninitialize();
		delete pData;
		pData = nullptr;
		return true;
	}
	catch (...)
	{
		std::cout << "[!] Error at excel_close().\n";
		return false;
	}
}
