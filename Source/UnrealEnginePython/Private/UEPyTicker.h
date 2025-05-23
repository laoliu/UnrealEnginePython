// Copyright 20Tab S.r.l.

#pragma once

#include "UEPyModule.h"
#include "Runtime/Core/Public/Containers/Ticker.h"

typedef struct
{
	PyObject_HEAD
		/* Type-specific fields go here. */
	bool garbaged;
	TSharedPtr<FPythonSmartDelegate> delegate_ptr;
#if ENGINE_MAJOR_VERSION == 5
	FTSTicker::FDelegateHandle dhandle;
#else
		FDelegateHandle dhandle;
#endif
} ue_PyFDelegateHandle;

PyObject *py_unreal_engine_add_ticker(PyObject *, PyObject *);
PyObject *py_unreal_engine_remove_ticker(PyObject *, PyObject *);

void ue_python_init_fdelegatehandle(PyObject *);