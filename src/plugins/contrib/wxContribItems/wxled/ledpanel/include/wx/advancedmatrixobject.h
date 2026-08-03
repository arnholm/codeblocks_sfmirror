/***************************************************************
 * Name:      advancedmatrixobejct.h
 * Purpose:   Defines Class AdvancedMatrixObject
 * Author:    Christian Gr�fe (info@mcs-soft.de)
 * Created:   2007-03-02
 * Copyright: Christian Gr�fe (www.mcs-soft.de)
 * License:	  wxWindows licence
 **************************************************************/

#ifndef ADVANCEDMATRIXOBJECT_H
#define ADVANCEDMATRIXOBJECT_H

#include "wx/matrixobject.h"

#ifdef __WXMSW__
    #ifndef DLLEXPORT
	      #define DLLEXPORT __declspec (dllexport)
    #endif
#else
    #define DLLEXPORT
#endif

class DLLEXPORT AdvancedMatrixObject : public MatrixObject
{
	public:
		AdvancedMatrixObject();
		AdvancedMatrixObject(const MatrixObject& mo);
		AdvancedMatrixObject(const char* data, int width, int height=0);
		virtual ~AdvancedMatrixObject();

		// Rotierefunktionen
		void RotateLeft();
		void RotateRight();

		// Shiftfunktionen
		void ShiftLeft();
		void ShiftRight();
		void ShiftUp();
		void ShiftDown();

		// Fitfunktionen
		void FitLeft();
		void FitRight();
		void FitTop();
		void FitBottom();

		// F�llfunktionen
		void FillLine(int y, char d);
		void FillRow(int x, char d);
		void FillAll(char d);

		// L�schfunktionen
		void ClearLine(int y);
		void ClearRow(int x);

		//

};



#endif // ADVANCEDMATRIXOBJECT_H
