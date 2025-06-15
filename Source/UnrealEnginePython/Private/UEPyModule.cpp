#include "UEPyModule.h"
#include "Logging/LogMacros.h"

#include "UEPyCallable.h"
DEFINE_LOG_CATEGORY(LogPython);


// 初始化函数：注册 FKey 类型到 Python 模块
PyTypeObject* ue_python_get_object_type()
{
	static PyTypeObject* key_type;
	if (key_type)
		return key_type;

	PyObject* module_name = PyUnicode_FromString("unreal");
	if (!module_name) return nullptr;

	//PyObject* new_unreal_engine_module1 = PyImport_GetModuleDict();
	PyObject* ue_module = PyImport_GetModule(module_name);
	// 获取模块字典
	PyObject* py_module_dict = PyModule_GetDict(ue_module);
	if (!py_module_dict) {
		PyErr_SetString(PyExc_RuntimeError, "Failed to get module dictionary");
		return nullptr;
	}

	// 获取Key类对象
	PyObject* py_key_class = PyDict_GetItemString(py_module_dict, "Object");
	if (!py_key_class) {
		PyErr_SetString(PyExc_KeyError, "Key class not found in module dictionary");
		return nullptr;
	}

	// 验证是否为类型对象
	if (!PyType_Check(py_key_class)) {
		PyErr_SetString(PyExc_TypeError, "Key is not a type object");
		return nullptr;
	}

	// 转换为PyTypeObject指针
	key_type = (PyTypeObject*)py_key_class;

	return (key_type);
}
ue_PyUObject* ue_get_python_uobject(UObject* ue_obj)
{
	if (!ue_obj)
		return nullptr;

	ue_PyUObject* ret = FUnrealEnginePythonHouseKeeper::Get()->GetPyUObject(ue_obj);
	if (!ret)
	{
#if ENGINE_MAJOR_VERSION == 5
		if (!ue_obj->IsValidLowLevel() || ue_obj->IsUnreachable())
#else
		if (!ue_obj->IsValidLowLevel() || ue_obj->IsPendingKillOrUnreachable())
#endif
			return nullptr;

		ue_PyUObject* ue_py_object = (ue_PyUObject*)PyObject_New(ue_PyUObject, ue_python_get_object_type());
		if (!ue_py_object)
		{
			return nullptr;
		}
		ue_py_object->ue_object = ue_obj;
		ue_py_object->py_proxy = nullptr;
		ue_py_object->auto_rooted = 0;
		ue_py_object->py_dict = PyDict_New();
		ue_py_object->owned = 0;

		FUnrealEnginePythonHouseKeeper::Get()->RegisterPyUObject(ue_obj, ue_py_object);

#if defined(UEPY_MEMORY_DEBUG)
		UE_LOG(LogPython, Warning, TEXT("CREATED UPyObject at %p for %p %s"), ue_py_object, ue_obj, *ue_obj->GetName());
#endif
		return ue_py_object;
	}
	return ret;

}

void unreal_engine_py_log_error()
{
	PyObject* type = NULL;
	PyObject* value = NULL;
	PyObject* traceback = NULL;

	PyErr_Fetch(&type, &value, &traceback);
	PyErr_NormalizeException(&type, &value, &traceback);

	if (!value)
	{
		PyErr_Clear();
		return;
	}

	char* msg = NULL;
#if PY_MAJOR_VERSION >= 3
	PyObject* zero = PyUnicode_AsUTF8String(PyObject_Str(value));
	if (zero)
	{
		msg = PyBytes_AsString(zero);
	}
#else
	msg = PyString_AsString(PyObject_Str(value));
#endif
	if (!msg)
	{
		PyErr_Clear();
		return;
	}

	UE_LOG(LogPython, Error, TEXT("%s"), UTF8_TO_TCHAR(msg));

	// taken from uWSGI ;)
	if (!traceback)
	{
		PyErr_Clear();
		return;
	}

	PyObject* traceback_module = PyImport_ImportModule("traceback");
	if (!traceback_module)
	{
		PyErr_Clear();
		return;
	}

	PyObject* traceback_dict = PyModule_GetDict(traceback_module);
	PyObject* format_exception = PyDict_GetItemString(traceback_dict, "format_exception");

	if (format_exception)
	{
		PyObject* ret = PyObject_CallFunctionObjArgs(format_exception, type, value, traceback, NULL);
		if (!ret)
		{
			PyErr_Clear();
			return;
		}
		if (PyList_Check(ret))
		{
			for (int i = 0; i < PyList_Size(ret); i++)
			{
				PyObject* item = PyList_GetItem(ret, i);
				if (item)
				{
					UE_LOG(LogPython, Error, TEXT("%s"), UTF8_TO_TCHAR(UEPyUnicode_AsUTF8(PyObject_Str(item))));
				}
			}
		}
		else
		{
			UE_LOG(LogPython, Error, TEXT("%s"), UTF8_TO_TCHAR(UEPyUnicode_AsUTF8(PyObject_Str(ret))));
		}
	}

	PyErr_Clear();
}

// convert a property to a python object
PyObject* ue_py_convert_property(FProperty* prop, uint8* buffer, int32 index)
{
	if (auto casted_prop = CastField<FBoolProperty>(prop))
	{
		bool value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		if (value)
		{
			Py_RETURN_TRUE;
		}
		Py_RETURN_FALSE;
	}

	if (auto casted_prop = CastField<FIntProperty>(prop))
	{
		int value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		return PyLong_FromLong(value);
	}

	if (auto casted_prop = CastField<FUInt32Property>(prop))
	{
		uint32 value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		return PyLong_FromUnsignedLong(value);
	}

	if (auto casted_prop = CastField<FInt64Property>(prop))
	{
		long long value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		return PyLong_FromLongLong(value);
	}

	// this is likely a bug - it was a FInt64Property before
	if (auto casted_prop = CastField<FUInt64Property>(prop))
	{
		uint64 value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		return PyLong_FromUnsignedLongLong(value);
	}

	if (auto casted_prop = CastField<FFloatProperty>(prop))
	{
		float value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		return PyFloat_FromDouble(value);
	}

	if (auto casted_prop = CastField<FDoubleProperty>(prop))
	{
		double value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		return PyFloat_FromDouble(value);
	}

	if (auto casted_prop = CastField<FByteProperty>(prop))
	{
		uint8 value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		return PyLong_FromUnsignedLong(value);
	}

	if (auto casted_prop = CastField<FEnumProperty>(prop))
	{
		void* prop_addr = casted_prop->ContainerPtrToValuePtr<void>(buffer, index);
		uint64 enum_index = casted_prop->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(prop_addr);
		return PyLong_FromUnsignedLong(enum_index);
	}

	if (auto casted_prop = CastField<FStrProperty>(prop))
	{
		FString value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		return PyUnicode_FromString(TCHAR_TO_UTF8(*value));
	}

	if (auto casted_prop = CastField<FTextProperty>(prop))
	{
		FText value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		return PyUnicode_FromString(TCHAR_TO_UTF8(*value.ToString()));
	}

	if (auto casted_prop = CastField<FNameProperty>(prop))
	{
		FName value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		return PyUnicode_FromString(TCHAR_TO_UTF8(*value.ToString()));
	}

	if (auto casted_prop = CastField<FObjectPropertyBase>(prop))
	{
		auto value = casted_prop->GetObjectPropertyValue_InContainer(buffer, index);
		if (value)
		{
			Py_RETURN_UOBJECT(value);
		}
		Py_RETURN_NONE;
	}

	if (auto casted_prop = CastField<FClassProperty>(prop))
	{
		auto value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		if (value)
		{
			Py_RETURN_UOBJECT(value);
		}
		return PyErr_Format(PyExc_Exception, "invalid UClass type for %s", TCHAR_TO_UTF8(*casted_prop->GetName()));
	}

	// try to manage known struct first
	if (auto casted_prop = CastField<FStructProperty>(prop))
	{
		if (auto casted_struct = Cast<UScriptStruct>(casted_prop->Struct))
		{
			/*if (casted_struct == TBaseStructure<FVector>::Get())
			{
				FVector vec = *casted_prop->ContainerPtrToValuePtr<FVector>(buffer, index);
				return py_ue_new_fvector(vec);
			}
			if (casted_struct == TBaseStructure<FVector2D>::Get())
			{
				FVector2D vec = *casted_prop->ContainerPtrToValuePtr<FVector2D>(buffer, index);
				return py_ue_new_fvector2d(vec);
			}
			if (casted_struct == TBaseStructure<FRotator>::Get())
			{
				FRotator rot = *casted_prop->ContainerPtrToValuePtr<FRotator>(buffer, index);
				return py_ue_new_frotator(rot);
			}
			if (casted_struct == TBaseStructure<FTransform>::Get())
			{
				FTransform transform = *casted_prop->ContainerPtrToValuePtr<FTransform>(buffer, index);
				return py_ue_new_ftransform(transform);
			}
			if (casted_struct == FHitResult::StaticStruct())
			{
				FHitResult hit = *casted_prop->ContainerPtrToValuePtr<FHitResult>(buffer, index);
				return py_ue_new_fhitresult(hit);
			}
			if (casted_struct == TBaseStructure<FColor>::Get())
			{
				FColor color = *casted_prop->ContainerPtrToValuePtr<FColor>(buffer, index);
				return py_ue_new_fcolor(color);
			}
			if (casted_struct == TBaseStructure<FLinearColor>::Get())
			{
				FLinearColor color = *casted_prop->ContainerPtrToValuePtr<FLinearColor>(buffer, index);
				return py_ue_new_flinearcolor(color);
			}
			return py_ue_new_uscriptstruct(casted_struct, casted_prop->ContainerPtrToValuePtr<uint8>(buffer, index));*/
		}
		return PyErr_Format(PyExc_TypeError, "unsupported UStruct type");
	}

	if (auto casted_prop = CastField<FWeakObjectProperty>(prop))
	{
		auto value = casted_prop->GetPropertyValue_InContainer(buffer, index);
		UObject* strong_obj = value.Get();
		if (strong_obj)
		{
			Py_RETURN_UOBJECT(strong_obj);
		}
		// nullptr
		Py_RETURN_NONE;
	}

	if (auto casted_prop = CastField<FMulticastDelegateProperty>(prop))
	{
		Py_RETURN_FPROPERTY(casted_prop);
	}

	if (auto casted_prop = CastField<FDelegateProperty>(prop))
	{
		Py_RETURN_FPROPERTY(casted_prop);
	}

	if (auto casted_prop = CastField<FArrayProperty>(prop))
	{
		FScriptArrayHelper_InContainer array_helper(casted_prop, buffer, index);

		FProperty* array_prop = casted_prop->Inner;

		// check for TArray<uint8>, so we can use bytearray optimization
		if (auto uint8_tarray = CastField<FByteProperty>(array_prop))
		{
			uint8* buf = array_helper.GetRawPtr();
			return PyByteArray_FromStringAndSize((char*)buf, array_helper.Num());
		}

		PyObject* py_list = PyList_New(0);

		for (int i = 0; i < array_helper.Num(); i++)
		{
			PyObject* item = ue_py_convert_property(array_prop, array_helper.GetRawPtr(i), 0);
			if (!item)
			{
				Py_DECREF(py_list);
				return NULL;
			}
			PyList_Append(py_list, item);
			Py_DECREF(item);
		}

		return py_list;
	}

	if (auto casted_prop = CastField<FMapProperty>(prop))
	{
		FScriptMapHelper_InContainer map_helper(casted_prop, buffer, index);

		PyObject* py_dict = PyDict_New();

		for (int32 i = 0; i < map_helper.Num(); i++)
		{
			if (map_helper.IsValidIndex(i))
			{

				uint8* ptr = map_helper.GetPairPtr(i);

				PyObject* py_key = ue_py_convert_property(map_helper.KeyProp, ptr, 0);
				if (!py_key)
				{
					Py_DECREF(py_dict);
					return NULL;
				}

				PyObject* py_value = ue_py_convert_property(map_helper.ValueProp, ptr, 0);
				if (!py_value)
				{
					Py_DECREF(py_dict);
					return NULL;
				}

				PyDict_SetItem(py_dict, py_key, py_value);
				Py_DECREF(py_key);
				Py_DECREF(py_value);
			}
		}

		return py_dict;
	}

	if (auto casted_prop = CastField<FSetProperty>(prop))
	{
		FScriptSetHelper_InContainer set_helper(casted_prop, buffer, index);

		FProperty* set_prop = casted_prop->ElementProp;

		PyObject* py_set = PySet_New(NULL);

		for (int i = 0; i < set_helper.GetMaxIndex(); i++)
		{
			if (set_helper.IsValidIndex(i))
			{
				PyObject* item = ue_py_convert_property(set_prop, set_helper.GetElementPtr(i), 0);
				if (!item)
				{
					Py_DECREF(py_set);
					return NULL;
				}
				PySet_Add(py_set, item);
				Py_DECREF(item);
			}
		}

		return py_set;
	}

	return PyErr_Format(PyExc_Exception, "unsupported value type %s for property %s", TCHAR_TO_UTF8(*prop->GetClass()->GetName()), TCHAR_TO_UTF8(*prop->GetName()));
}

ue_PyUObject* ue_get_python_uobject_inc(UObject* ue_obj)
{
	ue_PyUObject* ret = ue_get_python_uobject(ue_obj);
	if (ret)
	{
		Py_INCREF(ret);
	}
	return ret;
}

// so for the moment lets reimplement this using fproperty
ue_PyFProperty* ue_get_python_fproperty(FProperty* ue_fprop)
{
	if (!ue_fprop)
		return nullptr;

	// so Im still confused - the argument was a UObject class

	// so for UObjects we store them in the housekeeping object
	// which we lookup here - what to do??

	//ue_PyUObject* ret = FUnrealEnginePythonHouseKeeper::Get()->GetPyUObject(ue_obj);

	ue_PyFProperty* ret = nullptr;

	if (!ret)
	{

		/*ue_PyFProperty* ue_py_property = (ue_PyFProperty*)PyObject_New(ue_PyFProperty, &ue_PyFPropertyType);
		if (!ue_py_property)
		{
			return nullptr;
		}*/
		// so we must initialize the type struct variables
		//ue_py_property->ue_fproperty = ue_fprop;
		//ue_py_property->py_proxy = nullptr;
		//ue_py_property->py_dict = PyDict_New();
		//ue_py_property->auto_rooted = 0;
		//ue_py_property->owned = 0;
#if defined(UEPY_MEMORY_DEBUG)
		UE_LOG(LogPython, Warning, TEXT("CREATED UPyFProperty at %p for %p %s"), ue_py_property, ue_fprop, *ue_fprop->GetName());
#endif
		//return ue_py_property;
	}
	return ret;
}


ue_PyFProperty* ue_get_python_fproperty_inc(FProperty* ue_fprop)
{
	ue_PyFProperty* ret = ue_get_python_fproperty(ue_fprop);
	if (ret)
	{
		Py_INCREF(ret);
	}
	return ret;
}
// check if a python object is a wrapper to a UObject
ue_PyUObject* ue_is_pyuobject(PyObject* obj)
{
	//EXTRA_UE_LOG(LogPython, Warning, TEXT("ue_is_pyuobject: in obj %p type %p"), obj, Py_TYPE((PyObject*)& ue_PyUObjectType));
	if (!PyObject_IsInstance(obj, (PyObject*)ue_python_get_object_type()))
		return nullptr;
	return (ue_PyUObject*)obj;
}

void ue_bind_events_for_py_class_by_attribute(UObject* u_obj, PyObject* py_class)
{
	// attempt to register events
	PyObject* attrs = PyObject_Dir(py_class);
	if (!attrs)
		return;

	AActor* actor = Cast<AActor>(u_obj);
	if (!actor)
	{
		UActorComponent* component = Cast<UActorComponent>(u_obj);
		if (!component)
			return;
		actor = component->GetOwner();
	}

	Py_ssize_t len = PyList_Size(attrs);
	for (Py_ssize_t i = 0; i < len; i++)
	{
		PyObject* py_attr_name = PyList_GetItem(attrs, i);
		if (!py_attr_name || !PyUnicodeOrString_Check(py_attr_name))
			continue;
		PyObject* item = PyObject_GetAttrString(py_class, UEPyUnicode_AsUTF8(py_attr_name));
		if (item && PyCallable_Check(item))
		{
			// check for ue_event signature
			PyObject* event_signature = PyObject_GetAttrString(item, (char*)"ue_event");
			if (event_signature)
			{
				if (PyUnicodeOrString_Check(event_signature))
				{
					FString event_name = FString(UTF8_TO_TCHAR(UEPyUnicode_AsUTF8(event_signature)));
					TArray<FString> parts;
					int n = event_name.ParseIntoArray(parts, UTF8_TO_TCHAR("."));
					if (n < 1 || n > 2)
					{
						PyErr_SetString(PyExc_Exception, "invalid ue_event syntax, must be the name of an event or ComponentName.Event");
						unreal_engine_py_log_error();
					}
					else
					{
						if (n == 1)
						{
							if (!ue_bind_pyevent(ue_get_python_uobject(actor), parts[0], item, true))
							{
								unreal_engine_py_log_error();
							}
						}
						else
						{
							bool found = false;
							for (UActorComponent* component : actor->GetComponents())
							{
								if (component->GetFName() == FName(*parts[0]))
								{
									if (!ue_bind_pyevent(ue_get_python_uobject(component), parts[1], item, true))
									{
										unreal_engine_py_log_error();
									}
									found = true;
									break;
								}
							}

							if (!found)
							{
								PyErr_SetString(PyExc_Exception, "unable to find component by name");
								unreal_engine_py_log_error();
							}
						}
					}
				}
				else
				{
					PyErr_SetString(PyExc_Exception, "ue_event attribute must be a string");
					unreal_engine_py_log_error();
				}
			}
			Py_XDECREF(event_signature);
		}
		Py_XDECREF(item);
	}
	Py_DECREF(attrs);

	PyErr_Clear();
}

// automatically bind events based on class methods names
void ue_autobind_events_for_pyclass(ue_PyUObject* u_obj, PyObject* py_class)
{
	PyObject* attrs = PyObject_Dir(py_class);
	if (!attrs)
		return;

	Py_ssize_t len = PyList_Size(attrs);
	for (Py_ssize_t i = 0; i < len; i++)
	{
		PyObject* py_attr_name = PyList_GetItem(attrs, i);
		if (!py_attr_name || !PyUnicodeOrString_Check(py_attr_name))
			continue;
		FString attr_name = UTF8_TO_TCHAR(UEPyUnicode_AsUTF8(py_attr_name));
		if (!attr_name.StartsWith("on_", ESearchCase::CaseSensitive))
			continue;
		// check if the attr is a callable
		PyObject* item = PyObject_GetAttrString(py_class, TCHAR_TO_UTF8(*attr_name));
		if (item && PyCallable_Check(item))
		{
			TArray<FString> parts;
			if (attr_name.ParseIntoArray(parts, UTF8_TO_TCHAR("_")) > 1)
			{
				FString event_name;
				for (FString part : parts)
				{
					FString first_letter = part.Left(1).ToUpper();
					part.RemoveAt(0);
					event_name = event_name.Append(first_letter);
					event_name = event_name.Append(part);
				}
				// do not fail on wrong properties
				ue_bind_pyevent(u_obj, event_name, item, false);
			}
		}
		Py_XDECREF(item);
	}

	Py_DECREF(attrs);
}

PyObject* ue_bind_pyevent(ue_PyUObject* u_obj, FString event_name, PyObject* py_callable, bool fail_on_wrong_property)
{

#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
	FProperty* f_property = u_obj->ue_object->GetClass()->FindPropertyByName(FName(*event_name));
	if (!f_property)
#else
	UProperty* u_property = u_obj->ue_object->GetClass()->FindPropertyByName(FName(*event_name));
	if (!u_property)
#endif
	{
		if (fail_on_wrong_property)
			return PyErr_Format(PyExc_Exception, "unable to find event property %s", TCHAR_TO_UTF8(*event_name));
		Py_RETURN_NONE;
	}

#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
	if (auto casted_prop = CastField<FMulticastDelegateProperty>(f_property))
#else
	if (auto casted_prop = Cast<UMulticastDelegateProperty>(u_property))
#endif
	{
#if !(ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 23))
		FMulticastScriptDelegate multiscript_delegate = casted_prop->GetPropertyValue_InContainer(u_obj->ue_object);
#elif !(ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25))
		FMulticastScriptDelegate multiscript_delegate = *casted_prop->GetMulticastDelegate(u_obj->ue_object);
#endif

		FScriptDelegate script_delegate;
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
		// can we reuse UPythonDelegate here??
		UPythonDelegate* py_delegate = FUnrealEnginePythonHouseKeeper::Get()->NewDelegate(u_obj->ue_object, py_callable, casted_prop->SignatureFunction);
#else
		UPythonDelegate* py_delegate = FUnrealEnginePythonHouseKeeper::Get()->NewDelegate(u_obj->ue_object, py_callable, casted_prop->SignatureFunction);
#endif
		// fake UFUNCTION for bypassing checks
		script_delegate.BindUFunction(py_delegate, FName("PyFakeCallable"));

		// add the new delegate
#if !(ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25))
		multiscript_delegate.Add(script_delegate);
#else
		casted_prop->AddDelegate(script_delegate, u_obj->ue_object);
#endif

		// re-assign multicast delegate
#if !(ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 23))
		casted_prop->SetPropertyValue_InContainer(u_obj->ue_object, multiscript_delegate);
#elif !(ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25))
		casted_prop->SetMulticastDelegate(u_obj->ue_object, multiscript_delegate);
#endif
	}
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
	else if (auto casted_prop_delegate = CastField<FDelegateProperty>(f_property))
#else
	else if (auto casted_prop_delegate = Cast<UDelegateProperty>(u_property))
#endif
	{

		FScriptDelegate script_delegate = casted_prop_delegate->GetPropertyValue_InContainer(u_obj->ue_object);
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
		// can we reuse UPythonDelegate here??
		UPythonDelegate* py_delegate = FUnrealEnginePythonHouseKeeper::Get()->NewDelegate(u_obj->ue_object, py_callable, casted_prop_delegate->SignatureFunction);
#else
		UPythonDelegate* py_delegate = FUnrealEnginePythonHouseKeeper::Get()->NewDelegate(u_obj->ue_object, py_callable, casted_prop_delegate->SignatureFunction);
#endif
		// fake UFUNCTION for bypassing checks
		script_delegate.BindUFunction(py_delegate, FName("PyFakeCallable"));

		// re-assign multicast delegate
		casted_prop_delegate->SetPropertyValue_InContainer(u_obj->ue_object, script_delegate);
	}
	else
	{
		if (fail_on_wrong_property)
			return PyErr_Format(PyExc_Exception, "property %s is not an event", TCHAR_TO_UTF8(*event_name));
	}

	Py_RETURN_NONE;
}

static void py_ue_destroy_params(UFunction* u_function, uint8* buffer)
{
	// destroy params
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
	TFieldIterator<FProperty> DArgs(u_function);
	for (; DArgs && (DArgs->PropertyFlags & CPF_Parm); ++DArgs)
	{
		FProperty* prop = *DArgs;
		prop->DestroyValue_InContainer(buffer);
	}
#else
	TFieldIterator<UProperty> DArgs(u_function);
	for (; DArgs && (DArgs->PropertyFlags & CPF_Parm); ++DArgs)
	{
		UProperty* prop = *DArgs;
		prop->DestroyValue_InContainer(buffer);
	}
#endif
}

PyObject* py_ue_ufunction_call(UFunction* u_function, UObject* u_obj, PyObject* args, int argn, PyObject* kwargs)
{

	// check for __super call
	if (kwargs)
	{
		PyObject* is_super_call = PyDict_GetItemString(kwargs, (char*)"__super");
		if (is_super_call)
		{
			if (!u_function->GetSuperFunction())
			{
				return PyErr_Format(PyExc_Exception, "UFunction has no SuperFunction");
			}
			u_function = u_function->GetSuperFunction();
		}
	}

	//NOTE: u_function->PropertiesSize maps to local variable uproperties + ufunction paramaters uproperties
	uint8* buffer = (uint8*)FMemory_Alloca(u_function->ParmsSize);
	FMemory::Memzero(buffer, u_function->ParmsSize);
	// initialize args
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
	for (TFieldIterator<FProperty> IArgs(u_function); IArgs && IArgs->HasAnyPropertyFlags(CPF_Parm); ++IArgs)
#else
	for (TFieldIterator<UProperty> IArgs(u_function); IArgs && IArgs->HasAnyPropertyFlags(CPF_Parm); ++IArgs)
#endif
	{
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
		FProperty* prop = *IArgs;
#else
		UProperty* prop = *IArgs;
#endif
		if (!prop->HasAnyPropertyFlags(CPF_ZeroConstructor))
		{
			prop->InitializeValue_InContainer(buffer);
		}

		//UObject::CallFunctionByNameWithArguments() only does this part on non return value params
		if ((IArgs->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm)
		{
			if (!prop->IsInContainer(u_function->ParmsSize))
			{
				return PyErr_Format(PyExc_Exception, "Attempting to import func param property that's out of bounds. %s", TCHAR_TO_UTF8(*u_function->GetName()));
			}
#if WITH_EDITOR
			FString default_key = FString("CPP_Default_") + prop->GetName();
			FString default_key_value = u_function->GetMetaData(FName(*default_key));
			if (!default_key_value.IsEmpty())
			{
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 17)
				prop->ImportText_Direct(*default_key_value, prop->ContainerPtrToValuePtr<uint8>(buffer), nullptr, PPF_None, NULL);
#else
				prop->ImportText(*default_key_value, prop->ContainerPtrToValuePtr<uint8>(buffer), PPF_Localized, NULL);
#endif
			}
#endif
		}
	}


	Py_ssize_t tuple_len = PyTuple_Size(args);

	int has_out_params = 0;

#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
	TFieldIterator<FProperty> PArgs(u_function);
#else
	TFieldIterator<UProperty> PArgs(u_function);
#endif
	for (; PArgs && ((PArgs->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm); ++PArgs)
	{
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
		FProperty* prop = *PArgs;
#else
		UProperty* prop = *PArgs;
#endif
		if (argn < tuple_len)
		{
			PyObject* py_arg = PyTuple_GetItem(args, argn);
			if (!py_arg)
			{
				py_ue_destroy_params(u_function, buffer);
				return PyErr_Format(PyExc_TypeError, "unable to get pyobject for property %s", TCHAR_TO_UTF8(*prop->GetName()));
			}
			if (!ue_py_convert_pyobject(py_arg, prop, buffer, 0))
			{
				py_ue_destroy_params(u_function, buffer);
				return PyErr_Format(PyExc_TypeError, "unable to convert pyobject to property %s (%s)", TCHAR_TO_UTF8(*prop->GetName()), TCHAR_TO_UTF8(*prop->GetClass()->GetName()));
			}
		}
		else if (kwargs)
		{
			PyObject* dict_value = PyDict_GetItemString(kwargs, TCHAR_TO_UTF8(*prop->GetName()));
			if (dict_value)
			{
				if (!ue_py_convert_pyobject(dict_value, prop, buffer, 0))
				{
					py_ue_destroy_params(u_function, buffer);
					return PyErr_Format(PyExc_TypeError, "unable to convert pyobject to property %s (%s)", TCHAR_TO_UTF8(*prop->GetName()), TCHAR_TO_UTF8(*prop->GetClass()->GetName()));
				}
			}
		}
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
		if (prop->HasAnyPropertyFlags(CPF_OutParm) && (prop->IsA<FArrayProperty>() || prop->HasAnyPropertyFlags(CPF_ConstParm) == false))
#else
		if (prop->HasAnyPropertyFlags(CPF_OutParm) && (prop->IsA<UArrayProperty>() || prop->HasAnyPropertyFlags(CPF_ConstParm) == false))
#endif
		{
			has_out_params++;
		}
		argn++;
	}

	FScopeCycleCounterUObject ObjectScope(u_obj);
	FScopeCycleCounterUObject FunctionScope(u_function);

	Py_BEGIN_ALLOW_THREADS;
	u_obj->ProcessEvent(u_function, buffer);
	Py_END_ALLOW_THREADS;

	PyObject* ret = nullptr;

	int has_ret_param = 0;
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
	TFieldIterator<FProperty> Props(u_function);
#else
	TFieldIterator<UProperty> Props(u_function);
#endif
	for (; Props; ++Props)
	{
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
		FProperty* prop = *Props;
#else
		UProperty* prop = *Props;
#endif
		if (prop->GetPropertyFlags() & CPF_ReturnParm)
		{
			ret = ue_py_convert_property(prop, buffer, 0);
			if (!ret)
			{
				// destroy params
				py_ue_destroy_params(u_function, buffer);
				return NULL;
			}
			has_ret_param = 1;
			break;
		}
	}

	if (has_out_params > 0)
	{
		PyObject* multi_ret = PyTuple_New(has_out_params + has_ret_param);
		if (ret)
		{
			PyTuple_SetItem(multi_ret, 0, ret);
		}
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
		TFieldIterator<FProperty> OProps(u_function);
#else
		TFieldIterator<UProperty> OProps(u_function);
#endif
		for (; OProps; ++OProps)
		{
#if ENGINE_MAJOR_VERSION == 5 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25)
			FProperty* prop = *OProps;
			if (prop->HasAnyPropertyFlags(CPF_OutParm) && (prop->IsA<FArrayProperty>() || prop->HasAnyPropertyFlags(CPF_ConstParm) == false))
#else
			UProperty* prop = *OProps;
			if (prop->HasAnyPropertyFlags(CPF_OutParm) && (prop->IsA<UArrayProperty>() || prop->HasAnyPropertyFlags(CPF_ConstParm) == false))
#endif
			{
				// skip return param as it must be always the first
				if (prop->GetPropertyFlags() & CPF_ReturnParm)
					continue;
				PyObject* py_out = ue_py_convert_property(prop, buffer, 0);
				if (!py_out)
				{
					Py_DECREF(multi_ret);
					// destroy params
					py_ue_destroy_params(u_function, buffer);
					return NULL;
				}
				PyTuple_SetItem(multi_ret, has_ret_param, py_out);
				has_ret_param++;
			}
		}
		// destroy params
		py_ue_destroy_params(u_function, buffer);
		return multi_ret;
	}

	// destroy params
	py_ue_destroy_params(u_function, buffer);

	if (ret)
		return ret;

	Py_RETURN_NONE;
}

// convert a python object to a property
bool ue_py_convert_pyobject(PyObject* py_obj, FProperty* prop, uint8* buffer, int32 index)
{

	if (PyBool_Check(py_obj))
	{
		auto casted_prop = CastField<FBoolProperty>(prop);
		if (!casted_prop)
			return false;
		if (PyObject_IsTrue(py_obj))
		{
			casted_prop->SetPropertyValue_InContainer(buffer, true, index);
		}
		else
		{
			casted_prop->SetPropertyValue_InContainer(buffer, false, index);
		}
		return true;
	}

	if (PyNumber_Check(py_obj))
	{
		if (auto casted_prop = CastField<FIntProperty>(prop))
		{
			PyObject* py_long = PyNumber_Long(py_obj);
			casted_prop->SetPropertyValue_InContainer(buffer, PyLong_AsLong(py_long), index);
			Py_DECREF(py_long);
			return true;
		}
		if (auto casted_prop = CastField<FUInt32Property>(prop))
		{
			PyObject* py_long = PyNumber_Long(py_obj);
			casted_prop->SetPropertyValue_InContainer(buffer, PyLong_AsUnsignedLong(py_long), index);
			Py_DECREF(py_long);
			return true;
		}
		if (auto casted_prop = CastField<FInt64Property>(prop))
		{
			PyObject* py_long = PyNumber_Long(py_obj);
			casted_prop->SetPropertyValue_InContainer(buffer, PyLong_AsLongLong(py_long), index);
			Py_DECREF(py_long);
			return true;
		}
		if (auto casted_prop = CastField<FInt64Property>(prop))
		{
			PyObject* py_long = PyNumber_Long(py_obj);
			casted_prop->SetPropertyValue_InContainer(buffer, PyLong_AsUnsignedLongLong(py_long), index);
			Py_DECREF(py_long);
			return true;
		}
		if (auto casted_prop = CastField<FFloatProperty>(prop))
		{
			PyObject* py_float = PyNumber_Float(py_obj);
			casted_prop->SetPropertyValue_InContainer(buffer, PyFloat_AsDouble(py_float), index);
			Py_DECREF(py_float);
			return true;
		}
		if (auto casted_prop = CastField<FDoubleProperty>(prop))
		{
			PyObject* py_float = PyNumber_Float(py_obj);
			casted_prop->SetPropertyValue_InContainer(buffer, PyFloat_AsDouble(py_float), index);
			Py_DECREF(py_float);
			return true;
		}
		if (auto casted_prop = CastField<FByteProperty>(prop))
		{
			PyObject* py_long = PyNumber_Long(py_obj);
			casted_prop->SetPropertyValue_InContainer(buffer, PyLong_AsUnsignedLong(py_long), index);
			Py_DECREF(py_long);
			return true;
		}
		if (auto casted_prop = CastField<FEnumProperty>(prop))
		{
			PyObject* py_long = PyNumber_Long(py_obj);
			void* prop_addr = casted_prop->ContainerPtrToValuePtr<void>(buffer, index);
			casted_prop->GetUnderlyingProperty()->SetIntPropertyValue(prop_addr, (uint64)PyLong_AsUnsignedLong(py_long));
			Py_DECREF(py_long);
			return true;
		}

		return false;
	}

	if (PyUnicodeOrString_Check(py_obj))
	{
		if (auto casted_prop = CastField<FStrProperty>(prop))
		{
			casted_prop->SetPropertyValue_InContainer(buffer, UTF8_TO_TCHAR(UEPyUnicode_AsUTF8(py_obj)), index);
			return true;
		}
		if (auto casted_prop = CastField<FNameProperty>(prop))
		{
			casted_prop->SetPropertyValue_InContainer(buffer, UTF8_TO_TCHAR(UEPyUnicode_AsUTF8(py_obj)), index);
			return true;
		}
		if (auto casted_prop = CastField<FTextProperty>(prop))
		{
			casted_prop->SetPropertyValue_InContainer(buffer, FText::FromString(UTF8_TO_TCHAR(UEPyUnicode_AsUTF8(py_obj))), index);
			return true;
		}
		return false;
	}

	if (PyBytes_Check(py_obj))
	{
		if (auto casted_prop = CastField<FArrayProperty>(prop))
		{
			FScriptArrayHelper_InContainer helper(casted_prop, buffer, index);

			if (auto item_casted_prop = CastField<FByteProperty>(casted_prop->Inner))
			{

				Py_ssize_t pybytes_len = PyBytes_Size(py_obj);
				uint8* buf = (uint8*)PyBytes_AsString(py_obj);


				// fix array helper size
				if (helper.Num() < pybytes_len)
				{
					helper.AddValues(pybytes_len - helper.Num());
				}
				else if (helper.Num() > pybytes_len)
				{
					helper.RemoveValues(pybytes_len, helper.Num() - pybytes_len);
				}


				FMemory::Memcpy(helper.GetRawPtr(), buf, pybytes_len);
				return true;
			}
		}

		return false;
	}

	if (PyByteArray_Check(py_obj))
	{
		if (auto casted_prop = CastField<FArrayProperty>(prop))
		{
			FScriptArrayHelper_InContainer helper(casted_prop, buffer, index);

			if (auto item_casted_prop = CastField<FByteProperty>(casted_prop->Inner))
			{

				Py_ssize_t pybytes_len = PyByteArray_Size(py_obj);
				uint8* buf = (uint8*)PyByteArray_AsString(py_obj);


				// fix array helper size
				if (helper.Num() < pybytes_len)
				{
					helper.AddValues(pybytes_len - helper.Num());
				}
				else if (helper.Num() > pybytes_len)
				{
					helper.RemoveValues(pybytes_len, helper.Num() - pybytes_len);
				}


				FMemory::Memcpy(helper.GetRawPtr(), buf, pybytes_len);

				return true;
			}
		}

		return false;
	}

	if (PyList_Check(py_obj))
	{
		if (auto casted_prop = CastField<FArrayProperty>(prop))
		{
			FScriptArrayHelper_InContainer helper(casted_prop, buffer, index);

			FProperty* array_prop = casted_prop->Inner;
			Py_ssize_t pylist_len = PyList_Size(py_obj);

			// fix array helper size
			if (helper.Num() < pylist_len)
			{
				helper.AddValues(pylist_len - helper.Num());
			}
			else if (helper.Num() > pylist_len)
			{
				helper.RemoveValues(pylist_len, helper.Num() - pylist_len);
			}

			for (int i = 0; i < (int)pylist_len; i++)
			{
				PyObject* py_item = PyList_GetItem(py_obj, i);
				if (!ue_py_convert_pyobject(py_item, array_prop, helper.GetRawPtr(i), 0))
				{
					return false;
				}
			}
			return true;
		}

		return false;
	}

	if (PyTuple_Check(py_obj))
	{
		if (auto casted_prop = CastField<FArrayProperty>(prop))
		{
			FScriptArrayHelper_InContainer helper(casted_prop, buffer, index);

			FProperty* array_prop = casted_prop->Inner;
			Py_ssize_t pytuple_len = PyTuple_Size(py_obj);

			// fix array helper size
			if (helper.Num() < pytuple_len)
			{
				helper.AddValues(pytuple_len - helper.Num());
			}
			else if (helper.Num() > pytuple_len)
			{
				helper.RemoveValues(pytuple_len, helper.Num() - pytuple_len);
			}

			for (int i = 0; i < (int)pytuple_len; i++)
			{
				PyObject* py_item = PyTuple_GetItem(py_obj, i);
				if (!ue_py_convert_pyobject(py_item, array_prop, helper.GetRawPtr(i), 0))
				{
					return false;
				}
			}
			return true;
		}

		return false;
	}

	if (PyDict_Check(py_obj))
	{
		if (auto casted_prop = CastField<FMapProperty>(prop))
		{
			FScriptMapHelper_InContainer map_helper(casted_prop, buffer, index);

			PyObject* py_key = nullptr;
			PyObject* py_value = nullptr;
			Py_ssize_t pos = 0;

			map_helper.EmptyValues();
			while (PyDict_Next(py_obj, &pos, &py_key, &py_value))
			{

				int32 hindex = map_helper.AddDefaultValue_Invalid_NeedsRehash();
				uint8* ptr = map_helper.GetPairPtr(hindex);

				if (!ue_py_convert_pyobject(py_key, casted_prop->KeyProp, ptr, 0))
				{
					return false;
				}

				if (!ue_py_convert_pyobject(py_value, casted_prop->ValueProp, ptr, 0))
				{
					return false;
				}
			}
			map_helper.Rehash();

			return true;
		}

		return false;
	}

	if (PySet_Check(py_obj))
	{
		if (auto casted_prop = CastField<FSetProperty>(prop))
		{
			FScriptSetHelper_InContainer set_helper(casted_prop, buffer, index);

			set_helper.EmptyElements();

			Py_ssize_t Size = PySet_Size(py_obj);

			TArray<PyObject*> Objects;

			Objects.Reset(Size);

			while (Size > 0)
			{
				PyObject* py_item = PySet_Pop(py_obj);

				int32 hindex = set_helper.AddDefaultValue_Invalid_NeedsRehash();

				uint8* ptr = set_helper.GetElementPtr(hindex);

				if (!ue_py_convert_pyobject(py_item, casted_prop->ElementProp, ptr, 0))
				{
					return false;
				}

				Objects.Add(py_item);

				--Size;
			}

			for (auto Object : Objects)
			{
				PySet_Add(py_obj, Object);
			}

			set_helper.Rehash();

			return true;
		}

		return false;
	}

	// structs

	//if (ue_PyFVector* py_vec = py_ue_is_fvector(py_obj))
	//{
	//	if (auto casted_prop = CastField<FStructProperty>(prop))
	//	{
	//		if (casted_prop->Struct == TBaseStructure<FVector>::Get())
	//		{
	//			*casted_prop->ContainerPtrToValuePtr<FVector>(buffer, index) = py_vec->vec;
	//			return true;
	//		}
	//	}
	//	return false;
	//}

	//if (ue_PyFVector2D* py_vec = py_ue_is_fvector2d(py_obj))
	//{
	//	if (auto casted_prop = CastField<FStructProperty>(prop))
	//	{
	//		if (casted_prop->Struct == TBaseStructure<FVector2D>::Get())
	//		{
	//			*casted_prop->ContainerPtrToValuePtr<FVector2D>(buffer, index) = py_vec->vec;
	//			return true;
	//		}
	//	}
	//	return false;
	//}

	//if (ue_PyFRotator* py_rot = py_ue_is_frotator(py_obj))
	//{
	//	if (auto casted_prop = CastField<FStructProperty>(prop))
	//	{
	//		if (casted_prop->Struct == TBaseStructure<FRotator>::Get())
	//		{
	//			*casted_prop->ContainerPtrToValuePtr<FRotator>(buffer, index) = py_rot->rot;
	//			return true;
	//		}
	//	}
	//	return false;
	//}

	//if (ue_PyFTransform* py_transform = py_ue_is_ftransform(py_obj))
	//{
	//	if (auto casted_prop = CastField<FStructProperty>(prop))
	//	{
	//		if (casted_prop->Struct == TBaseStructure<FTransform>::Get())
	//		{
	//			*casted_prop->ContainerPtrToValuePtr<FTransform>(buffer, index) = py_transform->transform;
	//			return true;
	//		}
	//	}
	//	return false;
	//}

	//if (ue_PyFColor* py_color = py_ue_is_fcolor(py_obj))
	//{
	//	if (auto casted_prop = CastField<FStructProperty>(prop))
	//	{
	//		if (casted_prop->Struct == TBaseStructure<FColor>::Get())
	//		{

	//			*casted_prop->ContainerPtrToValuePtr<FColor>(buffer, index) = py_color->color;
	//			return true;
	//		}
	//	}
	//	return false;
	//}

	//if (ue_PyFLinearColor* py_color = py_ue_is_flinearcolor(py_obj))
	//{
	//	if (auto casted_prop = CastField<FStructProperty>(prop))
	//	{
	//		if (casted_prop->Struct == TBaseStructure<FLinearColor>::Get())
	//		{
	//			*casted_prop->ContainerPtrToValuePtr<FLinearColor>(buffer, index) = py_color->color;
	//			return true;
	//		}
	//	}
	//	return false;
	//}

	//if (ue_PyFHitResult* py_hit = py_ue_is_fhitresult(py_obj))
	//{
	//	if (auto casted_prop = CastField<FStructProperty>(prop))
	//	{
	//		if (casted_prop->Struct == FHitResult::StaticStruct())
	//		{
	//			*casted_prop->ContainerPtrToValuePtr<FHitResult>(buffer, index) = py_hit->hit;
	//			return true;
	//		}
	//	}
	//	return false;
	//}

	//// generic structs
	//if (py_ue_is_uscriptstruct(py_obj))
	//{
	//	ue_PyUScriptStruct* py_u_struct = (ue_PyUScriptStruct*)py_obj;
	//	if (auto casted_prop = CastField<FStructProperty>(prop))
	//	{
	//		if (casted_prop->Struct == py_u_struct->u_struct)
	//		{
	//			uint8* dest = casted_prop->ContainerPtrToValuePtr<uint8>(buffer, index);
	//			py_u_struct->u_struct->InitializeStruct(dest);
	//			py_u_struct->u_struct->CopyScriptStruct(dest, py_u_struct->u_struct_ptr);
	//			return true;
	//		}
	//	}
	//	return false;
	//}

	EXTRA_UE_LOG(LogPython, Warning, TEXT("Convert Prop 1"));

	if (PyObject_IsInstance(py_obj, (PyObject*)ue_python_get_object_type()))
	{
		ue_PyUObject* ue_obj = (ue_PyUObject*)py_obj;
		EXTRA_UE_LOG(LogPython, Warning, TEXT("Convert Prop 2 is uobject %s"), *ue_obj->ue_object->GetName());
		if (ue_obj->ue_object->IsA<UClass>())
		{
			EXTRA_UE_LOG(LogPython, Warning, TEXT("Convert Prop 3 is class %s"), *ue_obj->ue_object->GetName());
			if (auto casted_prop = CastField<FClassProperty>(prop))
			{
				casted_prop->SetPropertyValue_InContainer(buffer, ue_obj->ue_object, index);
#ifdef EXTRA_DEBUG_CODE
				EXTRA_UE_LOG(LogPython, Warning, TEXT("Convert Prop 3a is uclass %s"), *ue_obj->ue_object->GetName());
				UK2Node_DynamicCast* node = (UK2Node_DynamicCast*)buffer;
				EXTRA_UE_LOG(LogPython, Warning, TEXT("Setting attr  targetype is %p"), (void*)(node->TargetType));
#endif
				return true;
			}
			else if (auto casted_prop_soft_class = CastField<FSoftClassProperty>(prop))
			{
				casted_prop_soft_class->SetPropertyValue_InContainer(buffer, FSoftObjectPtr(ue_obj->ue_object), index);
				return true;
			}
			else if (auto casted_prop_soft_object = CastField<FSoftObjectProperty>(prop))
			{

				casted_prop_soft_object->SetPropertyValue_InContainer(buffer, FSoftObjectPtr(ue_obj->ue_object), index);

				return true;
			}
			else if (auto casted_prop_weak_object = CastField<FWeakObjectProperty>(prop))
			{

				casted_prop_weak_object->SetPropertyValue_InContainer(buffer, FWeakObjectPtr(ue_obj->ue_object), index);

				return true;
			}
			else if (auto casted_prop_base = CastField<FObjectPropertyBase>(prop))
			{
				// ensure the object type is correct, otherwise crash could happen (soon or later)
				if (!ue_obj->ue_object->IsA(casted_prop_base->PropertyClass))
					return false;

				EXTRA_UE_LOG(LogPython, Warning, TEXT("Convert Prop 3d is uobject %s"), *ue_obj->ue_object->GetName());
				// (UObject *)buffer

				casted_prop_base->SetObjectPropertyValue_InContainer(buffer, ue_obj->ue_object, index);

				return true;
			}

			return false;
		}


		if (ue_obj->ue_object->IsA<UObject>())
		{
			EXTRA_UE_LOG(LogPython, Warning, TEXT("Convert Prop 4 is uobject %s"), *ue_obj->ue_object->GetName());
			if (auto casted_prop = CastField<FObjectPropertyBase>(prop))
			{
				// if the property specifies an interface, the object must be of a class that implements it
				if (casted_prop->PropertyClass->HasAnyClassFlags(CLASS_Interface))
				{
					if (!ue_obj->ue_object->GetClass()->ImplementsInterface(casted_prop->PropertyClass))
						return false;
				}
				else
				{
					// ensure the object type is correct, otherwise crash could happen (soon or later)
					if (!ue_obj->ue_object->IsA(casted_prop->PropertyClass))
						return false;
				}

				casted_prop->SetObjectPropertyValue_InContainer(buffer, ue_obj->ue_object, index);

				return true;
			}
			else if (auto casted_prop_soft_object = CastField<FSoftObjectProperty>(prop))
			{
				if (!ue_obj->ue_object->IsA(casted_prop_soft_object->PropertyClass))
					return false;

				casted_prop_soft_object->SetPropertyValue_InContainer(buffer, FSoftObjectPtr(ue_obj->ue_object), index);

				return true;
			}
			else if (auto casted_prop_interface = CastField<FInterfaceProperty>(prop))
			{
				// ensure the object type is correct, otherwise crash could happen (soon or later)
				if (!ue_obj->ue_object->GetClass()->ImplementsInterface(casted_prop_interface->InterfaceClass))
					return false;

#if ENGINE_MAJOR_VERSION == 5
				casted_prop_interface->SetPropertyValue_InContainer(buffer, TScriptInterface<IInterface>(ue_obj->ue_object), index);
#else
				casted_prop_interface->SetPropertyValue_InContainer(buffer, FScriptInterface(ue_obj->ue_object), index);
#endif

				return true;
			}
		}
		return false;
	}

	if (py_obj == Py_None)
	{
		auto casted_prop_class = CastField<FClassProperty>(prop);
		if (casted_prop_class)
		{

			casted_prop_class->SetPropertyValue_InContainer(buffer, nullptr, index);

			return true;
		}
		auto casted_prop = CastField<FObjectPropertyBase>(prop);
		if (casted_prop)
		{

			casted_prop->SetObjectPropertyValue_InContainer(buffer, nullptr, index);

			return true;
		}
		return false;
	}

	return false;

}