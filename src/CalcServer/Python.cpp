/*
 *  This file is part of AQUAgpusph, a free CFD program based on SPH.
 *  Copyright (C) 2012  Jose Luis Cercos Pita <jl.cercos@upm.es>
 *
 *  AQUAgpusph is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AQUAgpusph is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with AQUAgpusph.  If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 * @brief Python script execution tool.
 * (See Aqua::CalcServer::Python for details)
 */

#include <string.h>
#include <AuxiliarMethods.h>
#include <ScreenManager.h>
#include <CalcServer/Python.h>

namespace Aqua{ namespace CalcServer{

Python::Python(const char *tool_name, const char *script)
    : Tool(tool_name)
    , _script(NULL)
    , _module(NULL)
    , _func(NULL)
{
    _script = new char[strlen(script) + 1];
    strcpy(_script, script);
    // Look for a .py extension to remove it
    char *dot = strrchr(_script, '.');
    if(dot && !strcmp(dot, ".py")){
        strcpy(dot, "");
    }
}

Python::~Python()
{
    if(_script) delete[] _script; _script=NULL;
    if(_module) Py_DECREF(_module); _module=0;
    if(_func) Py_DECREF(_func); _func=0;
}

bool Python::setup()
{
    char msg[1024];
    InputOutput::ScreenManager *S = InputOutput::ScreenManager::singleton();

    sprintf(msg,
            "Loading the tool \"%s\"...\n",
            name());
    S->addMessageF(1, msg);

    if(initPython())
        return true;

    if(load())
        return true;

    return false;
}

bool Python::execute()
{
    InputOutput::ScreenManager *S = InputOutput::ScreenManager::singleton();
    PyObject *result;

    result = PyObject_CallObject(_func, NULL);
    if(!result) {
        S->addMessageF(3, "main() function execution failed.\n");
        printf("\n--- Python report --------------------------\n\n");
        PyErr_Print();
        printf("\n-------------------------- Python report ---\n");
        return true;
    }

    if(!PyObject_TypeCheck(result, &PyBool_Type)){
        S->addMessageF(3,
                       "main() function returned non boolean variable.\n");
        return true;
    }

    if(result == Py_False){
        S->addMessageF(3, "main() function returned False.\n");
        return true;
    }

    return false;
}

bool Python::initPython()
{
    InputOutput::ScreenManager *S = InputOutput::ScreenManager::singleton();

    if(Py_IsInitialized()){
        return false;
    }

    Py_Initialize();
    if(!Py_IsInitialized()){
        S->addMessageF(3, "Failure calling Py_Initialize().\n");
        return true;
    }

    PyRun_SimpleString("import sys");
    PyRun_SimpleString("import os");
    PyRun_SimpleString("curdir = os.getcwd()");
    PyRun_SimpleString("sys.path.append(curdir)");
    return false;
}

bool Python::load()
{
    InputOutput::ScreenManager *S = InputOutput::ScreenManager::singleton();
    char msg[1024];
    PyObject *modName;

    char comm[512];
    strcpy(comm, "");
    PyRun_SimpleString("curdir = os.getcwd()");
    const char *path = getFolderFromFilePath((const char*)_script);
    if(path[0]=='.')   // "./" prefix has been set
        sprintf(comm, "modulePath = curdir + \"%s\"", &path[1]);
    else
        sprintf(comm, "modulePath = \"%s\"", path);
    PyRun_SimpleString(comm);
    PyRun_SimpleString("sys.path.append(modulePath)");

    unsigned int start = strlen(path);
    if( (path[0]=='.') && (_script[0]!='.') )   // "./" prefix has been set
        start += 2;
    modName = PyUnicode_FromString(&_script[start]);
    _module = PyImport_Import(modName);
    Py_DECREF(modName); modName=0;
    if(!_module){
        sprintf(msg,
                "Python module \"%s\" cannot be imported.\n",
                &_script[start]);
        S->addMessageF(3, msg);
        printf("\n--- Python report --------------------------\n\n");
        PyErr_Print();
        printf("\n-------------------------- Python report ---\n");
        return true;
    }

    _func = PyObject_GetAttrString(_module, "main");
    if(!_func || !PyCallable_Check(_func)) {
        S->addMessageF(3, "main() function cannot be found.\n");
        printf("\n--- Python report --------------------------\n\n");
        PyErr_Print();
        printf("\n-------------------------- Python report ---\n");
        return true;
    }
    return false;
}

}}  // namespaces