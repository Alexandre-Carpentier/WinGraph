#include "Mouse.h"
#include "WinGraph.h"

POINT GetMousePosition(VOID)
{
	POINT Point;
	if (GetCursorPos(&Point) == FALSE)
		return { 0, 0 };

	return Point;
}

BOOL isMouseHover(HGRAPH hGraph)
{
	POINT Point ;
	if (GetCursorPos(&Point) == FALSE)
		return FALSE;

	RECT client;
	HWND hwndGraph = NULL;
	hwndGraph = GetGraphWnd(hGraph);
	if (hwndGraph)
	{
		if (IsWindowVisible(hwndGraph))
		{
			if (GetWindowRect(hwndGraph, &client))
			{
				//printf("\t%i>%i--%i<%i--%i<%i--%i>%i--\n", Point.x, client.left, Point.x, client.right, Point.y, client.bottom, Point.y, client.top);
				if (((Point.x > client.left) && (Point.x < client.right)) && ((Point.y < client.bottom) && (Point.y > client.top)))
				{
					//printf("Hoover\n");
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

VOID DisplayPointer(HGRAPH hGraph)
{
	if (isMouseHover(hGraph))
	{
	}
}
