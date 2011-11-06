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

#define _SV_SALDATA_CXX
#include <shell/kde_headers.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <poll.h>
#ifdef FREEBSD
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <osl/thread.h>
#include <osl/process.h>
#include <osl/module.h>

#include <tools/debug.hxx>

#include <vos/process.hxx>
#include <vos/mutex.hxx>

#include "unx/kde/kdedata.hxx"
#include "unx/i18n_im.hxx"
#include "unx/i18n_xkb.hxx"

#include "vclpluginapi.h"

/* #i59042# override KApplications method for session management
 * since it will interfere badly with our own.
 */
class VCLKDEApplication : public KApplication
{
    public:
    VCLKDEApplication() : KApplication() {}
    
    virtual void commitData(QSessionManager &sm);
};

void VCLKDEApplication::commitData(QSessionManager&)
{
}

/***************************************************************************
 * class SalKDEDisplay                                                     *
 ***************************************************************************/

SalKDEDisplay::SalKDEDisplay( Display* pDisp )
    : SalX11Display( pDisp )
{
}

SalKDEDisplay::~SalKDEDisplay()
{
    // in case never a frame opened
    static_cast<KDEXLib*>(GetXLib())->doStartup();
    // clean up own members
    doDestruct();
    // prevent SalDisplay from closing KApplication's display
    pDisp_ = NULL;
}

/***************************************************************************
 * class KDEXLib                                                           *
 ***************************************************************************/

KDEXLib::~KDEXLib()
{
    // #158056# on 64 bit linux using libXRandr.so.2 will crash in
    // XCloseDisplay when freeing extension data
    // no known work around, therefor currently leak. Hopefully
    // this does not make problems since we're shutting down anyway
    // should we ever get a real kde plugin that uses the KDE event loop
    // we should use kde's method to signal screen changes similar
    // to the gtk plugin
    #if ! defined USE_RANDR || ! (defined LINUX && defined X86_64)
    // properly deinitialize KApplication
    delete (VCLKDEApplication*)m_pApplication;
    #endif
    // free the faked cmdline arguments no longer needed by KApplication
    for( int i = 0; i < m_nFakeCmdLineArgs; i++ )
        free( m_pFreeCmdLineArgs[i] );
    delete [] m_pFreeCmdLineArgs;
    delete [] m_pAppCmdLineArgs;
}

void KDEXLib::Init()
{
	SalI18N_InputMethod* pInputMethod = new SalI18N_InputMethod;
	pInputMethod->SetLocale();
	XrmInitialize();

	KAboutData *kAboutData = new KAboutData( "OpenOffice.org",
			I18N_NOOP( "OpenOffice.org" ),
			"1.1.0",
			I18N_NOOP( "OpenOffice.org with KDE Native Widget Support." ),
			KAboutData::License_LGPL,
			"(c) 2003, 2004 Novell, Inc",
			I18N_NOOP( "OpenOffice.org is an office suite.\n" ),
			"http://kde.openoffice.org/index.html",
			"dev@kde.openoffice.org");
	kAboutData->addAuthor( "Jan Holesovsky",
			I18N_NOOP( "Original author and maintainer of the KDE NWF." ),
			"kendy@artax.karlin.mff.cuni.cz",
			"http://artax.karlin.mff.cuni.cz/~kendy" );

    m_nFakeCmdLineArgs = 1;
	sal_uInt16 nIdx;
	vos::OExtCommandLine aCommandLine;
	int nParams = aCommandLine.getCommandArgCount();
	rtl::OString aDisplay;
	rtl::OUString aParam, aBin;

	for ( nIdx = 0; nIdx < nParams; ++nIdx ) 
	{
		aCommandLine.getCommandArg( nIdx, aParam );
		if ( !m_pFreeCmdLineArgs && aParam.equalsAscii( "-display" ) && nIdx + 1 < nParams )
		{
			aCommandLine.getCommandArg( nIdx + 1, aParam );
			aDisplay = rtl::OUStringToOString( aParam, osl_getThreadTextEncoding() );

			m_nFakeCmdLineArgs = 3;
			m_pFreeCmdLineArgs = new char*[ m_nFakeCmdLineArgs ];
			m_pFreeCmdLineArgs[ 1 ] = strdup( "-display" );
			m_pFreeCmdLineArgs[ 2 ] = strdup( aDisplay.getStr() );
		}
	}
	if ( !m_pFreeCmdLineArgs )
		m_pFreeCmdLineArgs = new char*[ m_nFakeCmdLineArgs ];

    osl_getExecutableFile( &aParam.pData );
    osl_getSystemPathFromFileURL( aParam.pData, &aBin.pData );
    rtl::OString aExec = rtl::OUStringToOString( aBin, osl_getThreadTextEncoding() );
	m_pFreeCmdLineArgs[0] = strdup( aExec.getStr() );

    // make a copy of the string list for freeing it since
    // KApplication manipulates the pointers inside the argument vector
    // note: KApplication bad !
    m_pAppCmdLineArgs = new char*[ m_nFakeCmdLineArgs ];
    for( int i = 0; i < m_nFakeCmdLineArgs; i++ )
        m_pAppCmdLineArgs[i] = m_pFreeCmdLineArgs[i];

	KCmdLineArgs::init( m_nFakeCmdLineArgs, m_pAppCmdLineArgs, kAboutData );

	KApplication::disableAutoDcopRegistration();
	m_pApplication = new VCLKDEApplication();
    kapp->disableSessionManagement();
	
	Display* pDisp = QPaintDevice::x11AppDisplay();

	SalDisplay *pSalDisplay = new SalKDEDisplay( pDisp );

	pInputMethod->CreateMethod( pDisp );
	pInputMethod->AddConnectionWatch( pDisp, (void*)this );
	pSalDisplay->SetInputMethod( pInputMethod );

    PushXErrorLevel( true );
	SalI18N_KeyboardExtension *pKbdExtension = new SalI18N_KeyboardExtension( pDisp );
	XSync( pDisp, False );

	pKbdExtension->UseExtension( ! HasXErrorOccured() );
	PopXErrorLevel();

	pSalDisplay->SetKbdExtension( pKbdExtension );
}

void KDEXLib::doStartup()
{
    if( ! m_bStartupDone )
    {
        KStartupInfo::appStarted();
        m_bStartupDone = true;
        #if OSL_DEBUG_LEVEL > 1
        fprintf( stderr, "called KStartupInfo::appStarted()\n" );
        #endif
    }
}

/**********************************************************************
 * class KDEData                                                      *
 **********************************************************************/

KDEData::~KDEData()
{
}

void KDEData::Init()
{
    pXLib_ = new KDEXLib();
    pXLib_->Init();
}

/**********************************************************************
 * plugin entry point                                                 *
 **********************************************************************/

extern "C" {
    VCLPLUG_KDE_PUBLIC SalInstance* create_SalInstance( oslModule )
    {
        /* #i92121# workaround deadlocks in the X11 implementation
        */
        static const char* pNoXInitThreads = getenv( "SAL_NO_XINITTHREADS" );
        /* #i90094#
           from now on we know that an X connection will be
           established, so protect X against itself
        */
        if( ! ( pNoXInitThreads && *pNoXInitThreads ) )
            XInitThreads();

        rtl::OString aVersion( qVersion() );
#if OSL_DEBUG_LEVEL > 1
        fprintf( stderr, "qt version string is \"%s\"\n", aVersion.getStr() );
#endif
        sal_Int32 nIndex = 0, nMajor = 0, nMinor = 0, nMicro = 0;
        nMajor = aVersion.getToken( 0, '.', nIndex ).toInt32();
        if( nIndex > 0 )
            nMinor = aVersion.getToken( 0, '.', nIndex ).toInt32();
        if( nIndex > 0 )
            nMicro = aVersion.getToken( 0, '.', nIndex ).toInt32();
        if( nMajor != 3 || nMinor < 2 || (nMinor == 2 && nMicro < 2) )
        {
#if OSL_DEBUG_LEVEL > 1
            fprintf( stderr, "unsuitable qt version %d.%d.%d\n", (int)nMajor, (int)nMinor, (int)nMicro );
#endif
            return NULL;
        }
        
        KDESalInstance* pInstance = new KDESalInstance( new SalYieldMutex() );
#if OSL_DEBUG_LEVEL > 1
        fprintf( stderr, "created KDESalInstance 0x%p\n", pInstance );
#endif

        // initialize SalData
        KDEData *pSalData = new KDEData();
        SetSalData( pSalData );
        pSalData->m_pInstance = pInstance;
        pSalData->Init();
        pSalData->initNWF();

        return pInstance;
    }
}
