#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <float.h>

// Public declaration goes here
typedef VOID* HGRAPH;
#ifdef __cplusplus
extern "C" {
#endif
	__declspec(dllexport)BOOL StartGraph(HGRAPH hGraph);
	__declspec(dllexport)VOID StopGraph(HGRAPH hGraph);
	__declspec(dllexport)VOID FreeGraph(HGRAPH hGraph);
	__declspec(dllexport)HGRAPH CreateGraph(HWND hWnd, RECT GraphArea, INT SignalCount, INT BufferSize);
	__declspec(dllexport)VOID SetSignalLabel(HGRAPH hGraph, CONST CHAR* lpszLabel, INT iSignalNumber);
	__declspec(dllexport)VOID SetRecordingMode(HGRAPH hGraph, BOOL logging);
	__declspec(dllexport)VOID SetAutoscaleMode(HGRAPH hGraph, BOOL mode);
	__declspec(dllexport)VOID SetYminVal(HGRAPH hGraph, FLOAT ymin);
	__declspec(dllexport)VOID SetYmaxVal(HGRAPH hGraph, FLOAT ymax);
	__declspec(dllexport)VOID SetFilteringMode(HGRAPH hGraph, BOOL filtering);
	__declspec(dllexport)BOOL GetGraphState(HGRAPH hGraph);
	__declspec(dllexport)HGLRC GetGraphRC(HGRAPH);
	__declspec(dllexport)HDC GetGraphDC(HGRAPH);
	__declspec(dllexport)HWND GetGraphParentWnd(HGRAPH);
	__declspec(dllexport)HWND GetGraphWnd(HGRAPH hGraph);
	__declspec(dllexport)INT GetGraphSignalCount(HGRAPH hGraph);
	__declspec(dllexport)FLOAT GetGraphLastSignalValue(HGRAPH hGraph, INT SIGNB);

	__declspec(dllexport)VOID AddPoints(HGRAPH hGraph, float* y, INT PointsCount);
	__declspec(dllexport)BOOL Render(HGRAPH hGraph);
	__declspec(dllexport)VOID ReshapeGraph(HGRAPH hGraph, int left, int top, int right, int bottom);
#ifdef __cplusplus
}
#endif

#endif