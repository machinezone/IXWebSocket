/*
 *  IXCobraToPythonBot.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2020 Machine Zone, Inc. All rights reserved.
 */

#include "IXCobraToPythonBot.h"

#include "IXCobraBot.h"
#include "IXStatsdClient.h"
#include <chrono>
#include <ixcobra/IXCobraConnection.h>
#include <ixcore/utils/IXCoreLogger.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <map>
#include <cctype>

//
// I cannot get Windows to easily build on CI (github action) so support
// is disabled for now. It should be a simple fix 
// (linking error about missing debug build)
//

#ifdef IXBOTS_USE_PYTHON
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#endif

#ifdef IXBOTS_USE_PYTHON
namespace
{
    //
    // This function is unused at this point. It produce a correct output,
    // but triggers memory leaks when called repeateadly, as I cannot figure out how to
    // make the reference counting Python functions to work properly (Py_DECREF and friends)
    //
    PyObject* jsonToPythonObject(const Json::Value& val)
    {
        switch(val.type())
        {
            case Json::nullValue:
            {
                return Py_None;
            }

            case Json::intValue:
            {
                return PyLong_FromLong(val.asInt64());
            }

            case Json::uintValue:
            {
                return PyLong_FromLong(val.asUInt64());
            }

            case Json::realValue:
            {
                return PyFloat_FromDouble(val.asDouble());
            }

            case Json::stringValue:
            {
                return PyUnicode_FromString(val.asCString());
            }

            case Json::booleanValue:
            {
                return val.asBool() ? Py_True : Py_False;
            }

            case Json::arrayValue:
            {
                PyObject* list = PyList_New(val.size());
                Py_ssize_t i = 0;
                for (auto&& it = val.begin(); it != val.end(); ++it)
                {
                    PyList_SetItem(list, i++, jsonToPythonObject(*it));
                }
                return list;
            }

            case Json::objectValue:
            {
                PyObject* dict = PyDict_New();
                for (auto&& it = val.begin(); it != val.end(); ++it)
                {
                    PyObject* key = jsonToPythonObject(it.key());
                    PyObject* value = jsonToPythonObject(*it);

                    PyDict_SetItem(dict, key, value);
                }
                return dict;
            }
        }
    }
}
#endif

namespace ix
{
    int64_t cobra_to_python_bot(const ix::CobraBotConfig& config,
                                StatsdClient& statsdClient,
                                const std::string& scriptPath)
    {
#ifndef IXBOTS_USE_PYTHON
        CoreLogger::error("Command is disabled. "
                          "Needs to be configured with USE_PYTHON=1");
        return -1;
#else
        CobraBot bot;
        Py_InitializeEx(0); // 0 arg so that we do not install signal handlers 
                            // which prevent us from using Ctrl-C

        size_t lastIndex = scriptPath.find_last_of("."); 
        std::string modulePath = scriptPath.substr(0, lastIndex);

        PyObject* pyModuleName = PyUnicode_DecodeFSDefault(modulePath.c_str());

        if (pyModuleName == nullptr)
        {
            CoreLogger::error("Python error: Cannot decode file system path");
            PyErr_Print();
            return false;
        }

        // Import module
        PyObject* pyModule = PyImport_Import(pyModuleName);
        Py_DECREF(pyModuleName);
        if (pyModule == nullptr)
        {
            CoreLogger::error("Python error: Cannot import module.");
            CoreLogger::error("Module name cannot countain dash characters.");
            CoreLogger::error("Is PYTHONPATH set correctly ?");
            PyErr_Print();
            return false;
        }

        // module main funtion name is named 'run'
        const std::string entryPoint("run");
        PyObject* pyFunc = PyObject_GetAttrString(pyModule, entryPoint.c_str());

        if (!pyFunc)
        {
            CoreLogger::error("run symbol is missing from module.");
            PyErr_Print();
            return false;
        }

        if (!PyCallable_Check(pyFunc))
        {
            CoreLogger::error("run symbol is not a function.");
            PyErr_Print();
            return false;
        }

        bot.setOnBotMessageCallback(
            [&statsdClient, pyFunc]
                (const Json::Value& msg,
                 const std::string& /*position*/,
                 std::atomic<bool>& /*throttled*/,
                 std::atomic<bool>& fatalCobraError,
                 std::atomic<uint64_t>& sentCount) -> void {
            if (msg["device"].isNull())
            {
                CoreLogger::info("no device entry, skipping event");
                return;
            }

            if (msg["id"].isNull())
            {
                CoreLogger::info("no id entry, skipping event");
                return;
            }

            //
            // Invoke python script here. First build function parameters, a tuple
            //
            const int kVersion = 1; // We can bump this and let the interface evolve

            PyObject *pyArgs = PyTuple_New(2);
            PyTuple_SetItem(pyArgs, 0, PyLong_FromLong(kVersion)); // First argument

            //
            // It would be better to create a Python object (a dictionary) 
            // from the json msg, but it is simpler to serialize it to a string
            // and decode it on the Python side of the fence
            //
            PyObject* pySerializedJson = PyUnicode_FromString(msg.toStyledString().c_str());
            PyTuple_SetItem(pyArgs, 1, pySerializedJson); // Second argument

            // Invoke the python routine
            PyObject* pyList = PyObject_CallObject(pyFunc, pyArgs);

            // Error calling the function
            if (pyList == nullptr)
            {
                fatalCobraError = true;
                CoreLogger::error("run() function call failed. Input msg: ");
                auto serializedMsg = msg.toStyledString();
                CoreLogger::error(serializedMsg);
                PyErr_Print();
                CoreLogger::error("================");
                return;
            }

            // Invalid return type
            if (!PyList_Check(pyList))
            {
                fatalCobraError = true;
                CoreLogger::error("run() return type should be a list");
                return;
            }

            // The result is a list of dict containing sufficient info 
            // to send messages to statsd
            auto listSize = PyList_Size(pyList);
            
            for (Py_ssize_t i = 0 ; i < listSize; ++i)
            {
                PyObject* dict = PyList_GetItem(pyList, i);

                // Make sure this is a dict
                if (!PyDict_Check(dict))
                {
                    fatalCobraError = true;
                    CoreLogger::error("list element is not a dict");
                    continue;
                }

                //
                // Retrieve object kind
                //
                PyObject* pyKind = PyDict_GetItemString(dict, "kind");
                if (!PyUnicode_Check(pyKind))
                {
                    fatalCobraError = true;
                    CoreLogger::error("kind entry is not a string");
                    continue;
                }
                std::string kind(PyUnicode_AsUTF8(pyKind));

                bool counter = false;
                bool gauge = false;
                bool timing = false;

                if (kind == "counter")
                {
                    counter = true;
                }
                else if (kind == "gauge")
                {
                    gauge = true;
                }
                else if (kind == "timing")
                {
                    timing = true;
                }
                else
                {
                    fatalCobraError = true;
                    CoreLogger::error(std::string("invalid kind entry: ") + kind +
                                      ". Supported ones are counter, gauge, timing");
                    continue;
                }

                //
                // Retrieve object key
                //
                PyObject* pyKey = PyDict_GetItemString(dict, "key");
                if (!PyUnicode_Check(pyKey))
                {
                    fatalCobraError = true;
                    CoreLogger::error("key entry is not a string");
                    continue;
                }
                std::string key(PyUnicode_AsUTF8(pyKey));

                //
                // Retrieve object value and send data to statsd
                //
                PyObject* pyValue = PyDict_GetItemString(dict, "value");

                // Send data to statsd
                if (PyFloat_Check(pyValue))
                {
                    double value = PyFloat_AsDouble(pyValue);

                    if (counter)
                    {
                        statsdClient.count(key, value);
                    }
                    else if (gauge)
                    {
                        statsdClient.gauge(key, value);
                    }
                    else if (timing)
                    {
                        statsdClient.timing(key, value);
                    }
                }
                else if (PyLong_Check(pyValue))
                {
                    long value = PyLong_AsLong(pyValue);

                    if (counter)
                    {
                        statsdClient.count(key, value);
                    }
                    else if (gauge)
                    {
                        statsdClient.gauge(key, value);
                    }
                    else if (timing)
                    {
                        statsdClient.timing(key, value);
                    }
                }
                else
                {
                    fatalCobraError = true;
                    CoreLogger::error("value entry is neither an int or a float");
                    continue;
                }

                sentCount++; // should we update this for each statsd object sent ?
            }

            Py_DECREF(pyArgs);
            Py_DECREF(pyList);
        });

        bool status = bot.run(config);

        // Cleanup - we should do something similar in all exit case ...
        Py_DECREF(pyFunc);
        Py_DECREF(pyModule);
        Py_FinalizeEx();

        return status;
#endif
    }
}
