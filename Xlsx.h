#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <cmath>
#include <objbase.h>
#pragma comment(lib, "Ole32.lib")

#import "C:\Program Files (x86)\Common Files\Microsoft Shared\OFFICE14\MSO.dll" \
    rename("RGB","RGB_mso") rename("DocumentProperties","DocumentProperties_mso") 

//using namespace Office;

#import "C:\Program Files (x86)\Common Files\Microsoft Shared\VBA\VBA6\VBE6EXT.OLB"

//using namespace VBIDE;

#import "C:\Program Files (x86)\Microsoft Office\Office14\EXCEL.EXE" \
    rename( "DialogBox", "ExcelDialogBox" ) \
    rename( "RGB", "ExcelRGB" ) \
    rename( "CopyFile", "ExcelCopyFile" ) \
    rename( "ReplaceText", "ExcelReplaceText" ) \
    exclude( "IFont", "IPicture" )

typedef void* HEXCEL;

typedef struct {
    Excel::_ApplicationPtr inst;
    Excel::_WorksheetPtr pSheet;
    int line_nb;
}EXCELSTRUCT, *PEXCELSTRUCT;

HEXCEL excel_create_instance();
bool excel_save(HEXCEL hExcel, const char* filepath);
bool excel_addline(HEXCEL hExcel, char lpszLineTab[260]);
bool excel_additem(HEXCEL hExcel, char item[260], int x, int y);
bool excel_additem_on_current_line(HEXCEL hExcel, char item[260], int x);
bool excel_cariage_return(HEXCEL hExcel);
bool excel_drawgraph(HEXCEL hExcel);
bool excel_close(HEXCEL hExcel);
