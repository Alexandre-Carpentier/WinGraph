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
extern "C" __declspec(dllexport)BOOL StartGraph(HGRAPH hGraph);
extern "C" __declspec(dllexport)VOID StopGraph(HGRAPH hGraph);
extern "C" __declspec(dllexport)VOID FreeGraph(HGRAPH hGraph);
extern "C" __declspec(dllexport)HGRAPH CreateGraph(HWND hWnd, RECT GraphArea, INT SignalCount, INT BufferSize);
extern "C" __declspec(dllexport)VOID SetRecordingMode(HGRAPH hGraph, BOOL logging);
extern "C" __declspec(dllexport)BOOL GetGraphState(HGRAPH hGraph);
extern "C" __declspec(dllexport)HGLRC GetGraphRC(HGRAPH);
extern "C" __declspec(dllexport)HDC GetGraphDC(HGRAPH);
extern "C" __declspec(dllexport)HWND GetGraphParentWnd(HGRAPH);
extern "C" __declspec(dllexport)HWND GetGraphWnd(HGRAPH hGraph);
extern "C" __declspec(dllexport)INT GetGraphSignalCount(HGRAPH hGraph);

extern "C" __declspec(dllexport)VOID AddPoints(HGRAPH hGraph, float* y, INT PointsCount);
extern "C" __declspec(dllexport)BOOL Render(HGRAPH hGraph);
extern "C" __declspec(dllexport)VOID ReshapeGraph(HGRAPH hGraph, int left, int top, int right, int bottom);
#endif