#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <float.h>

enum FILTER_M{
	FILTER_NONE = 100,
	FILTER_HANNING,
	FILTER_BESEL,
	FILTER_EMA
};

enum LOGGER_M {
	LOGGER_NONE = 200,
	LOGGER_ASCII,
	LOGGER_TDMS,
	LOGGER_XLSX
};

// Public declaration goes here
typedef VOID* HGRAPH;
#ifdef __cplusplus
extern "C" {
#endif
	__declspec(dllexport)BOOL StartGraph(HGRAPH hGraph);
	__declspec(dllexport)VOID StopGraph(HGRAPH hGraph);
	__declspec(dllexport)VOID FreeGraph(HGRAPH *hGraph);
	__declspec(dllexport)HGRAPH CreateGraph(HWND hWnd, RECT GraphArea, INT SignalCount, INT BufferSize);
	__declspec(dllexport)BOOL SetSignalCount(HGRAPH hGraph, CONST INT iSignalNumber);
	__declspec(dllexport)VOID SetSignalLabel(HGRAPH hGraph, CONST CHAR szLabel[260], INT iSignalNumber); 
	__declspec(dllexport)VOID SetSignalColor(HGRAPH hGraph, INT R, INT G, INT B, INT iSignalNumber);
	__declspec(dllexport)VOID SetSignalVisible(HGRAPH hGraph, BOOL bDisplay, INT iSignalNumber); 
	__declspec(dllexport)VOID SetRecordingMode(HGRAPH hGraph, LOGGER_M logging);
	__declspec(dllexport)VOID SetAutoscaleMode(HGRAPH hGraph, BOOL mode);
	__declspec(dllexport)VOID SetDisplayCursor(HGRAPH hGraph, BOOL isActive);
	__declspec(dllexport)VOID SetYminVal(HGRAPH hGraph, double ymin); // Set the Y min axe value of the graph, autoscale clear this value when enable
	__declspec(dllexport)VOID SetYmaxVal(HGRAPH hGraph, double ymax); // Set the Y max axe value of the graph, autoscale clear this value when enable
	__declspec(dllexport)VOID SetZoomFactor(HGRAPH hGraph, int zoom); // Set the X scale factor value of the graph
	__declspec(dllexport)VOID SetFilteringMode(HGRAPH hGraph, FILTER_M filtering);
	__declspec(dllexport)VOID SetSignalMinValue(HGRAPH hGraph, INT SIGNB, DOUBLE val);
	__declspec(dllexport)VOID SetSignalAverageValue(HGRAPH hGraph, INT SIGNB, DOUBLE val);
	__declspec(dllexport)VOID SetSignalMaxValue(HGRAPH hGraph, INT SIGNB, DOUBLE val);
	__declspec(dllexport)BOOL GetGraphState(HGRAPH hGraph);
	__declspec(dllexport)HGLRC GetGraphRC(HGRAPH);
	__declspec(dllexport)HDC GetGraphDC(HGRAPH);
	__declspec(dllexport)HWND GetGraphParentWnd(HGRAPH);
	__declspec(dllexport)HWND GetGraphWnd(HGRAPH hGraph);
	__declspec(dllexport)INT GetGraphSignalCount(HGRAPH hGraph);
	__declspec(dllexport)INT GetZoomFactor(HGRAPH hGraph); 
	__declspec(dllexport)double GetGraphLastSignalValue(HGRAPH hGraph, INT SIGNB);
	__declspec(dllexport)double GetSignalMinValue(HGRAPH hGraph, INT SIGNB);
	__declspec(dllexport)double GetSignalAverageValue(HGRAPH hGraph, INT SIGNB);
	__declspec(dllexport)double GetSignalMaxValue(HGRAPH hGraph, INT SIGNB);
	__declspec(dllexport)VOID SignalResetStatisticValue(HGRAPH hGraph, INT SIGNB);

	__declspec(dllexport)VOID AddPoints(HGRAPH hGraph, double* y, INT PointsCount);
	__declspec(dllexport)BOOL Render(HGRAPH hGraph);
	__declspec(dllexport)VOID ReshapeGraph(HGRAPH hGraph, int left, int top, int right, int bottom);
#ifdef __cplusplus
}
#endif

#endif