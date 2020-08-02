/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This file contains code originally written for OpenCV, that can be found
 * in `modules/python/src2/cv2.cpp`.
 * OpenCV is licensed under 3BSD:
 *
 *                           License Agreement
 *                For Open Source Computer Vision Library
 *
 * Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
 * Copyright (C) 2009-2011, Willow Garage Inc., all rights reserved.
 * Third party copyrights are property of their respective owners.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *   * Redistribution's of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   * Redistribution's in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 *   * The name of the copyright holders may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * This software is provided by the copyright holders and contributors "as is" and
 * any express or implied warranties, including, but not limited to, the implied
 * warranties of merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the Intel Corporation or contributors be liable for any direct,
 * indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused
 * and on any theory of liability, whether in contract, strict liability,
 * or tort (including negligence or otherwise) arising in any way out of
 * the use of this software, even if advised of the possibility of such damage.
 **/

#include "cvmatndsliceconvert.h"
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/ndarrayobject.h>
#include <opencv2/core/core.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

using namespace cv;

static PyObject* opencv_error = nullptr;

class PyAllowThreads
{
public:
    PyAllowThreads() : _state(PyEval_SaveThread()) {}
    ~PyAllowThreads()
    {
        PyEval_RestoreThread(_state);
    }
private:
    PyThreadState* _state;
};

class PyEnsureGIL
{
public:
    PyEnsureGIL() : _state(PyGILState_Ensure()) {}
    ~PyEnsureGIL()
    {
        PyGILState_Release(_state);
    }
private:
    PyGILState_STATE _state;
};

PyObject *initNDArray()
{
    import_array()
    return NUMPY_IMPORT_ARRAY_RETVAL;
}

class NumpyAllocator : public MatAllocator
{
public:
    NumpyAllocator() { stdAllocator = Mat::getStdAllocator(); }
    ~NumpyAllocator() override {}

    UMatData* allocate(PyObject* o, int dims, const int* sizes, int type, size_t* step) const
    {
        UMatData* u = new UMatData(this);
        u->data = u->origdata = (uchar*)PyArray_DATA((PyArrayObject*) o);
        npy_intp* _strides = PyArray_STRIDES((PyArrayObject*) o);
        for( int i = 0; i < dims - 1; i++ )
            step[i] = (size_t)_strides[i];
        step[dims-1] = CV_ELEM_SIZE(type);
        u->size = (size_t) sizes[0] * step[0];
        u->userdata = o;
        return u;
    }

    UMatData* allocate(int dims0, const int* sizes, int type, void* data, size_t* step, AccessFlag flags, UMatUsageFlags usageFlags) const CV_OVERRIDE
    {
        if( data != nullptr )
        {
            // issue #6969: CV_Error(Error::StsAssert, "The data should normally be NULL!");
            // probably this is safe to do in such extreme case
            return stdAllocator->allocate(dims0, sizes, type, data, step, flags, usageFlags);
        }
        PyEnsureGIL gil;

        int depth = CV_MAT_DEPTH(type);
        int cn = CV_MAT_CN(type);
        const int f = (int)(sizeof(size_t)/8);
        int typenum = depth == CV_8U ? NPY_UBYTE : depth == CV_8S ? NPY_BYTE :
                                                                    depth == CV_16U ? NPY_USHORT : depth == CV_16S ? NPY_SHORT :
                                                                                                                     depth == CV_32S ? NPY_INT : depth == CV_32F ? NPY_FLOAT :
                                                                                                                                                                   depth == CV_64F ? NPY_DOUBLE : f*NPY_ULONGLONG + (f^1)*NPY_UINT;
        int i, dims = dims0;
        cv::AutoBuffer<npy_intp> _sizes((size_t) dims + 1);
        for( i = 0; i < dims; i++ )
            _sizes[i] = sizes[i];
        if( cn > 1 )
            _sizes[dims++] = cn;

        PyObject* o = PyArray_SimpleNew(dims, _sizes.data(), typenum);
        if(!o)
            CV_Error_(Error::StsError, ("The numpy array of typenum=%d, ndims=%d can not be created", typenum, dims));
        return allocate(o, dims0, sizes, type, step);
    }

    bool allocate(UMatData* u, AccessFlag accessFlags, UMatUsageFlags usageFlags) const CV_OVERRIDE
    {
        return stdAllocator->allocate(u, accessFlags, usageFlags);
    }

    void deallocate(UMatData* u) const CV_OVERRIDE
    {
        if(!u)
            return;
        PyEnsureGIL gil;
        CV_Assert(u->urefcount >= 0);
        CV_Assert(u->refcount >= 0);
        if(u->refcount == 0)
        {
            PyObject* o = (PyObject*)u->userdata;
            Py_XDECREF(o);
            delete u;
        }
    }

    const MatAllocator* stdAllocator;
};

static NumpyAllocator g_numpyAllocator;

PyObject* cvMatToNDArray(const cv::Mat& m)
{
    if( !m.data )
        Py_RETURN_NONE;
    Mat temp, *p = const_cast<Mat*>(&m);
    if(!p->u || p->allocator != &g_numpyAllocator) {
        temp.allocator = &g_numpyAllocator;
        try {
            m.copyTo(temp);
        } catch (const cv::Exception &e) {
            PyObject_SetAttrString(opencv_error, "file", PyUnicode_FromString(e.file.c_str()));
            PyObject_SetAttrString(opencv_error, "func", PyUnicode_FromString(e.func.c_str()));
            PyObject_SetAttrString(opencv_error, "line", PyLong_FromLong(e.line));
            PyObject_SetAttrString(opencv_error, "code", PyLong_FromLong(e.code));
            PyObject_SetAttrString(opencv_error, "msg", PyUnicode_FromString(e.msg.c_str()));
            PyObject_SetAttrString(opencv_error, "err", PyUnicode_FromString(e.err.c_str()));
            PyErr_SetString(opencv_error, e.what());
            Py_RETURN_NONE;
        }
        p = &temp;
    }

    PyObject* o = (PyObject*) p->u->userdata;
    Py_INCREF(o);
    return o;
}

cv::Mat cvMatFromNdArray(PyObject *o)
{
    cv::Mat m;
    bool allowND = true;
    if (!PyArray_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Argument is not a numpy array");
        if (!m.data)
            m.allocator = &g_numpyAllocator;
    } else {
        PyArrayObject* oarr = (PyArrayObject*) o;

        bool needcopy = false, needcast = false;
        int typenum = PyArray_TYPE(oarr), new_typenum = typenum;
        int type = typenum == NPY_UBYTE ? CV_8U : typenum == NPY_BYTE ? CV_8S :
                    typenum == NPY_USHORT ? CV_16U :
                    typenum == NPY_SHORT ? CV_16S :
                    typenum == NPY_INT ? CV_32S :
                    typenum == NPY_INT32 ? CV_32S :
                    typenum == NPY_FLOAT ? CV_32F :
                    typenum == NPY_DOUBLE ? CV_64F : -1;

        if (type < 0) {
            if (typenum == NPY_INT64 || typenum == NPY_UINT64
                    || type == NPY_LONG) {
                needcopy = needcast = true;
                new_typenum = NPY_INT;
                type = CV_32S;
            } else {
                PyErr_SetString(PyExc_TypeError, "Argument data type is not supported");
                m.allocator = &g_numpyAllocator;
                return m;
            }
        }

#ifndef CV_MAX_DIM
        const int CV_MAX_DIM = 32;
#endif

        int ndims = PyArray_NDIM(oarr);
        if (ndims >= CV_MAX_DIM) {
            PyErr_SetString(PyExc_TypeError, "Dimensionality of argument is too high");
            if (!m.data)
                m.allocator = &g_numpyAllocator;
            return m;
        }

        int size[CV_MAX_DIM + 1];
        size_t step[CV_MAX_DIM + 1];
        size_t elemsize = CV_ELEM_SIZE1(type);
        const npy_intp* _sizes = PyArray_DIMS(oarr);
        const npy_intp* _strides = PyArray_STRIDES(oarr);
        bool ismultichannel = ndims == 3 && _sizes[2] <= CV_CN_MAX;

        for (int i = ndims - 1; i >= 0 && !needcopy; i--) {
            // these checks handle cases of
            //  a) multi-dimensional (ndims > 2) arrays, as well as simpler 1- and 2-dimensional cases
            //  b) transposed arrays, where _strides[] elements go in non-descending order
            //  c) flipped arrays, where some of _strides[] elements are negative
            if ((i == ndims - 1 && (size_t) _strides[i] != elemsize)
                    || (i < ndims - 1 && _strides[i] < _strides[i + 1]))
                needcopy = true;
        }

        if (ismultichannel && _strides[1] != (npy_intp) elemsize * _sizes[2])
            needcopy = true;

        if (needcopy) {

            if (needcast) {
                o = PyArray_Cast(oarr, new_typenum);
                oarr = (PyArrayObject*) o;
            } else {
                oarr = PyArray_GETCONTIGUOUS(oarr);
                o = (PyObject*) oarr;
            }

            _strides = PyArray_STRIDES(oarr);
        }

        for (int i = 0; i < ndims; i++) {
            size[i] = (int) _sizes[i];
            step[i] = (size_t) _strides[i];
        }

        // handle degenerate case
        if (ndims == 0) {
            size[ndims] = 1;
            step[ndims] = elemsize;
            ndims++;
        }

        if (ismultichannel) {
            ndims--;
            type |= CV_MAKETYPE(0, size[2]);
        }

        if (ndims > 2 && !allowND) {
            PyErr_SetString(PyExc_TypeError, "Array has more than 2 dimensions");
        } else {

            m = Mat(ndims, size, type, PyArray_DATA(oarr), step);
            m.u = g_numpyAllocator.allocate(o, ndims, size, type, step);
            m.addref();

            if (!needcopy) {
                Py_INCREF(o);
            }
        }
        m.allocator = &g_numpyAllocator;
    }
    return m;
}

// warn about old-style casts again
#pragma GCC diagnostic pop
