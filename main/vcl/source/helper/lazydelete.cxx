/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/


// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_vcl.hxx"

#ifndef LAZYDELETE_CXX
#define LAZYDELETE_CXX

#include "vcl/window.hxx"
#include "vcl/menu.hxx"
#include "vcl/lazydelete.hxx"
#include "svdata.hxx"

namespace vcl {

LazyDeletorBase::LazyDeletorBase()
{
}

LazyDeletorBase::~LazyDeletorBase()
{
}

// instantiate instance pointers for LazyDeletor<Window,Menu>
template<> LazyDeletor<Window>* LazyDeletor<Window>::s_pOneInstance = NULL;
template<> LazyDeletor<Menu>* LazyDeletor<Menu>::s_pOneInstance = NULL;

// a list for all LazyeDeletor<T> singletons
static std::vector< LazyDeletorBase* > lcl_aDeletors;

void LazyDelete::addDeletor( LazyDeletorBase* i_pDel )
{
    lcl_aDeletors.push_back( i_pDel );
}

void LazyDelete::flush()
{
    unsigned int nCount = lcl_aDeletors.size();
    for( unsigned int i = 0; i < nCount; i++ )
        delete lcl_aDeletors[i];
    lcl_aDeletors.clear();
}

// specialized is_less function for Window
template<> bool LazyDeletor<Window>::is_less( Window* left, Window* right )
{
    return (left != right && right->IsChild( left, sal_True )) ? true : false;
}

// specialized is_less function for Menu
template<> bool LazyDeletor<Menu>::is_less( Menu* left, Menu* right )
{
    while( left && left != right )
        left = left->ImplGetStartedFrom();
    return left != NULL;
}

DeleteOnDeinitBase::~DeleteOnDeinitBase()
{
    ImplSVData* pSVData = ImplGetSVData();
    if( pSVData && pSVData->mpDeinitDeleteList != NULL )
        pSVData->mpDeinitDeleteList->remove( this );
}

void DeleteOnDeinitBase::addDeinitContainer( DeleteOnDeinitBase* i_pContainer )
{
    ImplSVData* pSVData = ImplGetSVData();
    if( ! pSVData )
    {
        ImplInitSVData();
        pSVData = ImplGetSVData();
    }

    DBG_ASSERT( ! pSVData->mbDeInit, "DeleteOnDeinit added after DeiInitVCL !" );
    if( pSVData->mbDeInit )
        return;
    
    if( pSVData->mpDeinitDeleteList == NULL )
        pSVData->mpDeinitDeleteList = new std::list< DeleteOnDeinitBase* >();
    pSVData->mpDeinitDeleteList->push_back( i_pContainer );
}

void DeleteOnDeinitBase::ImplDeleteOnDeInit()
{
    ImplSVData* pSVData = ImplGetSVData();    
    if( pSVData->mpDeinitDeleteList )
    {
        for( std::list< vcl::DeleteOnDeinitBase* >::iterator it = pSVData->mpDeinitDeleteList->begin();
             it != pSVData->mpDeinitDeleteList->end(); ++it )
        {
            (*it)->doCleanup();
        }
        delete pSVData->mpDeinitDeleteList;
        pSVData->mpDeinitDeleteList = NULL;
    }
}

} // namespace vcl

#endif

