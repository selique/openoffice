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
#include "precompiled_editeng.hxx"

//------------------------------------------------------------------------
//
// Global header
//
//------------------------------------------------------------------------

#include <limits.h>
#include <vector>
#include <algorithm>
#include <vos/mutex.hxx>
#include <vcl/window.hxx>
#include <vcl/svapp.hxx>
#include <editeng/flditem.hxx>
#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/awt/Point.hpp>
#include <com/sun/star/awt/Rectangle.hpp>
#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/accessibility/AccessibleTextType.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/accessibility/AccessibleEventId.hpp>
#include <comphelper/accessibleeventnotifier.hxx>
#include <comphelper/sequenceashashmap.hxx>
#include <unotools/accessiblestatesethelper.hxx>
#include <unotools/accessiblerelationsethelper.hxx>
#include <com/sun/star/accessibility/AccessibleRelationType.hpp>
#include <vcl/unohelp.hxx>
#include <editeng/editeng.hxx>
#include <editeng/unoprnms.hxx>
#include <editeng/unoipset.hxx>
#include <editeng/outliner.hxx>
#include <svl/intitem.hxx>

//------------------------------------------------------------------------
//
// Project-local header
//
//------------------------------------------------------------------------

#include <com/sun/star/beans/PropertyState.hpp>

//!!!#include <svx/unoshape.hxx>
//!!!#include <svx/dialmgr.hxx>
//!!!#include "accessibility.hrc"

#include <editeng/unolingu.hxx>
#include <editeng/unopracc.hxx>
#include "editeng/AccessibleEditableTextPara.hxx"
#include "AccessibleHyperlink.hxx"

#include <svtools/colorcfg.hxx>
//IAccessibility2 Implementation 2009-----
#ifndef _ACCESSIBLEHYPERLINK_HXX
#include <Accessiblehyperlink.hxx>
#endif
#include <algorithm>
using namespace std;
//-----IAccessibility2 Implementation 2009
#include <editeng/numitem.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/viewsh.hxx>

using namespace ::com::sun::star;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::accessibility;


//------------------------------------------------------------------------
//
// AccessibleEditableTextPara implementation
//
//------------------------------------------------------------------------

namespace accessibility
{
//IAccessibility2 Implementation 2009-----
	//Window* GetCurrentEditorWnd();
//-----IAccessibility2 Implementation 2009

    const SvxItemPropertySet* ImplGetSvxCharAndParaPropertiesSet()
    {
        // PropertyMap for character and paragraph properties
        static const SfxItemPropertyMapEntry aPropMap[] =
        {
			SVX_UNOEDIT_OUTLINER_PROPERTIES,
            SVX_UNOEDIT_CHAR_PROPERTIES,
            SVX_UNOEDIT_PARA_PROPERTIES,
            SVX_UNOEDIT_NUMBERING_PROPERTIE,
            {MAP_CHAR_LEN("TextUserDefinedAttributes"),     EE_CHAR_XMLATTRIBS,     &::getCppuType((const ::com::sun::star::uno::Reference< ::com::sun::star::container::XNameContainer >*)0)  ,        0,     0},
            {MAP_CHAR_LEN("ParaUserDefinedAttributes"),     EE_PARA_XMLATTRIBS,     &::getCppuType((const ::com::sun::star::uno::Reference< ::com::sun::star::container::XNameContainer >*)0)  ,        0,     0},
            {0,0,0,0,0,0}
        };
        static SvxItemPropertySet aPropSet( aPropMap, EditEngine::GetGlobalItemPool() );
        return &aPropSet;
    }


    DBG_NAME( AccessibleEditableTextPara )

    // --> OD 2006-01-11 #i27138# - add parameter <_pParaManager>
    AccessibleEditableTextPara::AccessibleEditableTextPara(
                                const uno::Reference< XAccessible >& rParent,
                                const AccessibleParaManager* _pParaManager )
        : AccessibleTextParaInterfaceBase( m_aMutex ),
          mnParagraphIndex( 0 ),
          mnIndexInParent( 0 ),
          mpEditSource( NULL ),
          maEEOffset( 0, 0 ),
          mxParent( rParent ),
          // well, that's strictly (UNO) exception safe, though not
          // really robust. We rely on the fact that this member is
          // constructed last, and that the constructor body catches
          // exceptions, thus no chance for exceptions once the Id is
          // fetched. Nevertheless, normally should employ RAII here...
          mnNotifierClientId(::comphelper::AccessibleEventNotifier::registerClient()),
          // --> OD 2006-01-11 #i27138#
          mpParaManager( _pParaManager )
          // <--
    {
#ifdef DBG_UTIL
        DBG_CTOR( AccessibleEditableTextPara, NULL );
        OSL_TRACE( "AccessibleEditableTextPara received ID: %d\n", mnNotifierClientId );
#endif

		try
        {
            // Create the state set.
            ::utl::AccessibleStateSetHelper* pStateSet  = new ::utl::AccessibleStateSetHelper ();
            mxStateSet = pStateSet;

            // these are always on
            pStateSet->AddState( AccessibleStateType::MULTI_LINE );
            pStateSet->AddState( AccessibleStateType::FOCUSABLE );
            pStateSet->AddState( AccessibleStateType::VISIBLE );
            pStateSet->AddState( AccessibleStateType::SHOWING );
            pStateSet->AddState( AccessibleStateType::ENABLED );
            pStateSet->AddState( AccessibleStateType::SENSITIVE );
        }
        catch( const uno::Exception& ) {}
    }

    AccessibleEditableTextPara::~AccessibleEditableTextPara()
    {
        DBG_DTOR( AccessibleEditableTextPara, NULL );

        // sign off from event notifier
        if( getNotifierClientId() != -1 )
        {
            try
            {
                ::comphelper::AccessibleEventNotifier::revokeClient( getNotifierClientId() );
#ifdef DBG_UTIL
                OSL_TRACE( "AccessibleEditableTextPara revoked ID: %d\n", mnNotifierClientId );
#endif
            }
            catch( const uno::Exception& ) {}
        }
    }

    ::rtl::OUString AccessibleEditableTextPara::implGetText()
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return GetTextRange( 0, GetTextLen() );
    }

    ::com::sun::star::lang::Locale AccessibleEditableTextPara::implGetLocale()
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        lang::Locale		aLocale;

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getLocale: paragraph index value overflow");

        // return locale of first character in the paragraph
        return SvxLanguageToLocale(aLocale, GetTextForwarder().GetLanguage( static_cast< sal_uInt16 >( GetParagraphIndex() ), 0 ));
    }

    void AccessibleEditableTextPara::implGetSelection( sal_Int32& nStartIndex, sal_Int32& nEndIndex )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        sal_uInt16 nStart, nEnd;

        if( GetSelection( nStart, nEnd ) )
        {
            nStartIndex = nStart;
            nEndIndex = nEnd;
        }
        else
        {
            // #102234# No exception, just set to 'invalid'
            nStartIndex = -1;
            nEndIndex = -1;
        }
    }

    void AccessibleEditableTextPara::implGetParagraphBoundary( ::com::sun::star::i18n::Boundary& rBoundary, sal_Int32 /*nIndex*/ )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );
        DBG_WARNING( "AccessibleEditableTextPara::implGetParagraphBoundary: only a base implementation, ignoring the index" );

        rBoundary.startPos = 0;
	//IAccessibility2 Implementation 2009-----   
        //rBoundary.endPos = GetTextLen();
        ::rtl::OUString sText( implGetText() );
        sal_Int32 nLength = sText.getLength();
        rBoundary.endPos = nLength;
	//-----IAccessibility2 Implementation 2009
    }

    void AccessibleEditableTextPara::implGetLineBoundary( ::com::sun::star::i18n::Boundary& rBoundary, sal_Int32 nIndex )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        SvxTextForwarder&	rCacheTF = GetTextForwarder();
        const sal_Int32		nParaIndex = GetParagraphIndex();

        DBG_ASSERT(nParaIndex >= 0 && nParaIndex <= USHRT_MAX,
                   "AccessibleEditableTextPara::implGetLineBoundary: paragraph index value overflow");

        const sal_Int32 nTextLen = rCacheTF.GetTextLen( static_cast< sal_uInt16 >( nParaIndex ) );

        CheckPosition(nIndex);

        rBoundary.startPos = rBoundary.endPos = -1;

        const sal_uInt16 nLineCount=rCacheTF.GetLineCount( static_cast< sal_uInt16 >( nParaIndex ) );

        if( nIndex == nTextLen )
        {
            // #i17014# Special-casing one-behind-the-end character
            if( nLineCount <= 1 )
                rBoundary.startPos = 0;
            else
                rBoundary.startPos = nTextLen - rCacheTF.GetLineLen( static_cast< sal_uInt16 >( nParaIndex ),
                                                                     nLineCount-1 );

            rBoundary.endPos = nTextLen;
        }
        else
        {
            // normal line search
            sal_uInt16 nLine;
            sal_Int32 nCurIndex;
            for( nLine=0, nCurIndex=0; nLine<nLineCount; ++nLine )
            {
                nCurIndex += rCacheTF.GetLineLen( static_cast< sal_uInt16 >( nParaIndex ), nLine);

                if( nCurIndex > nIndex )
                {
                    rBoundary.startPos = nCurIndex - rCacheTF.GetLineLen(static_cast< sal_uInt16 >( nParaIndex ), nLine);
                    rBoundary.endPos = nCurIndex;
                    break;
                }
            }
        }
    }

    int AccessibleEditableTextPara::getNotifierClientId() const
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return mnNotifierClientId;
    }

    void AccessibleEditableTextPara::SetIndexInParent( sal_Int32 nIndex )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        mnIndexInParent = nIndex;
    }

    sal_Int32 AccessibleEditableTextPara::GetIndexInParent() const
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return mnIndexInParent;
    }

    void AccessibleEditableTextPara::SetParagraphIndex( sal_Int32 nIndex )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        sal_Int32 nOldIndex = mnParagraphIndex;

        mnParagraphIndex = nIndex;

        WeakBullet::HardRefType aChild( maImageBullet.get() );
        if( aChild.is() )
            aChild->SetParagraphIndex(mnParagraphIndex);

        try
        {
            if( nOldIndex != nIndex )
            {
				uno::Any aOldDesc;
				uno::Any aOldName;

				try
				{
					aOldDesc <<= getAccessibleDescription();
					aOldName <<= getAccessibleName();
				}
				catch( const uno::Exception& ) {} // optional behaviour
                // index and therefore description changed
                FireEvent( AccessibleEventId::DESCRIPTION_CHANGED, uno::makeAny( getAccessibleDescription() ), aOldDesc );
                FireEvent( AccessibleEventId::NAME_CHANGED, uno::makeAny( getAccessibleName() ), aOldName );
            }
        }
        catch( const uno::Exception& ) {} // optional behaviour
    }

    sal_Int32 AccessibleEditableTextPara::GetParagraphIndex() const SAL_THROW((uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return mnParagraphIndex;
    }

    void AccessibleEditableTextPara::Dispose()
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        int nClientId( getNotifierClientId() );

        // #108212# drop all references before notifying dispose
        mxParent = NULL;
        mnNotifierClientId = -1;
        mpEditSource = NULL;

        // notify listeners
        if( nClientId != -1 )
        {
            try
            {
                uno::Reference < XAccessibleContext > xThis = getAccessibleContext();

                // #106234# Delegate to EventNotifier
                ::comphelper::AccessibleEventNotifier::revokeClientNotifyDisposing( nClientId, xThis );
#ifdef DBG_UTIL
                OSL_TRACE( "Disposed ID: %d\n", nClientId );
#endif
            }
            catch( const uno::Exception& ) {}
        }
    }

    void AccessibleEditableTextPara::SetEditSource( SvxEditSourceAdapter* pEditSource )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        WeakBullet::HardRefType aChild( maImageBullet.get() );
        if( aChild.is() )
            aChild->SetEditSource(pEditSource);

        if( !pEditSource ) 
        {
            // going defunc
            UnSetState( AccessibleStateType::SHOWING );
            UnSetState( AccessibleStateType::VISIBLE );
            SetState( AccessibleStateType::INVALID );
            SetState( AccessibleStateType::DEFUNC );

            Dispose();
        }
//IAccessibility2 Implementation 2009-----
		mpEditSource = pEditSource; 
//-----IAccessibility2 Implementation 2009
        // #108900# Init last text content
        try
        {
            TextChanged();
        }
        catch( const uno::RuntimeException& ) {}
    }

    ESelection AccessibleEditableTextPara::MakeSelection( sal_Int32 nStartEEIndex, sal_Int32 nEndEEIndex )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        // check overflow
        DBG_ASSERT(nStartEEIndex >= 0 && nStartEEIndex <= USHRT_MAX &&
                   nEndEEIndex >= 0 && nEndEEIndex <= USHRT_MAX &&
                   GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::MakeSelection: index value overflow");

		sal_uInt16 nParaIndex = static_cast< sal_uInt16 >( GetParagraphIndex() );
        return ESelection( nParaIndex, static_cast< sal_uInt16 >( nStartEEIndex ),
                           nParaIndex, static_cast< sal_uInt16 >( nEndEEIndex ) );
    }

    ESelection AccessibleEditableTextPara::MakeSelection( sal_Int32 nEEIndex )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return MakeSelection( nEEIndex, nEEIndex+1 );
    }

    ESelection AccessibleEditableTextPara::MakeCursor( sal_Int32 nEEIndex )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return MakeSelection( nEEIndex, nEEIndex );
    }

    void AccessibleEditableTextPara::CheckIndex( sal_Int32 nIndex ) SAL_THROW((lang::IndexOutOfBoundsException, uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        if( nIndex < 0 || nIndex >= getCharacterCount() )
            throw lang::IndexOutOfBoundsException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("AccessibleEditableTextPara: character index out of bounds")),
                                                  uno::Reference< uno::XInterface >
                                                  ( static_cast< ::cppu::OWeakObject* > (this) ) );	// disambiguate hierarchy
    }

    void AccessibleEditableTextPara::CheckPosition( sal_Int32 nIndex ) SAL_THROW((lang::IndexOutOfBoundsException, uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        if( nIndex < 0 || nIndex > getCharacterCount() )
            throw lang::IndexOutOfBoundsException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("AccessibleEditableTextPara: character position out of bounds")),
                                                  uno::Reference< uno::XInterface >
                                                  ( static_cast< ::cppu::OWeakObject* > (this) ) );	// disambiguate hierarchy
    }

    void AccessibleEditableTextPara::CheckRange( sal_Int32 nStart, sal_Int32 nEnd ) SAL_THROW((lang::IndexOutOfBoundsException, uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        CheckPosition( nStart );
        CheckPosition( nEnd );
    }

    sal_Bool AccessibleEditableTextPara::GetSelection( sal_uInt16& nStartPos, sal_uInt16& nEndPos ) SAL_THROW((uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ESelection aSelection;
        sal_uInt16 nPara = static_cast< sal_uInt16 > ( GetParagraphIndex() );
        if( !GetEditViewForwarder().GetSelection( aSelection ) )
            return sal_False;

        if( aSelection.nStartPara < aSelection.nEndPara )
        {
            if( aSelection.nStartPara > nPara ||
                aSelection.nEndPara < nPara )
                return sal_False;

            if( nPara == aSelection.nStartPara )
                nStartPos = aSelection.nStartPos;
            else
                nStartPos = 0;

            if( nPara == aSelection.nEndPara )
                nEndPos = aSelection.nEndPos;
            else
                nEndPos = GetTextLen();
        }
        else
        {
            if( aSelection.nStartPara < nPara ||
                aSelection.nEndPara > nPara )
                return sal_False;

            if( nPara == aSelection.nStartPara )
                nStartPos = aSelection.nStartPos;
            else
                nStartPos = GetTextLen();

            if( nPara == aSelection.nEndPara )
                nEndPos = aSelection.nEndPos;
            else
                nEndPos = 0;
        }

        return sal_True;
    }

    String AccessibleEditableTextPara::GetText( sal_Int32 nIndex ) SAL_THROW((uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return GetTextForwarder().GetText( MakeSelection(nIndex) );
    }

    String AccessibleEditableTextPara::GetTextRange( sal_Int32 nStartIndex, sal_Int32 nEndIndex ) SAL_THROW((uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return GetTextForwarder().GetText( MakeSelection(nStartIndex, nEndIndex) );
    }

    sal_uInt16 AccessibleEditableTextPara::GetTextLen() const SAL_THROW((uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return GetTextForwarder().GetTextLen( static_cast< sal_uInt16 >( GetParagraphIndex() ) );
    }

    sal_Bool AccessibleEditableTextPara::IsVisible() const
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return mpEditSource ? sal_True : sal_False ;
    }

    uno::Reference< XAccessibleText > AccessibleEditableTextPara::GetParaInterface( sal_Int32 nIndex )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        uno::Reference< XAccessible > xParent = getAccessibleParent();
        if( xParent.is() )
        {
            uno::Reference< XAccessibleContext > xParentContext = xParent->getAccessibleContext();
            if( xParentContext.is() )
            {
                uno::Reference< XAccessible > xPara = xParentContext->getAccessibleChild( nIndex );
                if( xPara.is() )
                {
                    return uno::Reference< XAccessibleText > ( xPara, uno::UNO_QUERY );
                }
            }
        }

        return uno::Reference< XAccessibleText >();
    }

    SvxEditSourceAdapter& AccessibleEditableTextPara::GetEditSource() const SAL_THROW((uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        if( mpEditSource )
            return *mpEditSource;
        else
            throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("No edit source, object is defunct")),
                                        uno::Reference< uno::XInterface >
                                        ( static_cast< ::cppu::OWeakObject* >
                                          ( const_cast< AccessibleEditableTextPara* > (this) ) ) );	// disambiguate hierarchy
    }

    SvxAccessibleTextAdapter& AccessibleEditableTextPara::GetTextForwarder() const SAL_THROW((uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        SvxEditSourceAdapter& rEditSource = GetEditSource();
        SvxAccessibleTextAdapter* pTextForwarder = rEditSource.GetTextForwarderAdapter();

        if( !pTextForwarder )
            throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Unable to fetch text forwarder, object is defunct")),
                                        uno::Reference< uno::XInterface >
                                        ( static_cast< ::cppu::OWeakObject* >
                                          ( const_cast< AccessibleEditableTextPara* > (this) ) ) );	// disambiguate hierarchy

        if( pTextForwarder->IsValid() )
            return *pTextForwarder;
        else
            throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Text forwarder is invalid, object is defunct")),
                                        uno::Reference< uno::XInterface >
                                        ( static_cast< ::cppu::OWeakObject* >
                                          ( const_cast< AccessibleEditableTextPara* > (this) ) ) );	// disambiguate hierarchy
    }

    SvxViewForwarder& AccessibleEditableTextPara::GetViewForwarder() const SAL_THROW((uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        SvxEditSource& rEditSource = GetEditSource();
        SvxViewForwarder* pViewForwarder = rEditSource.GetViewForwarder();

        if( !pViewForwarder )
        {
            throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Unable to fetch view forwarder, object is defunct")),
                                        uno::Reference< uno::XInterface >
                                        ( static_cast< ::cppu::OWeakObject* >
                                          ( const_cast< AccessibleEditableTextPara* > (this) ) ) );	// disambiguate hierarchy
        }

        if( pViewForwarder->IsValid() )
            return *pViewForwarder;
        else
            throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("View forwarder is invalid, object is defunct")),
                                        uno::Reference< uno::XInterface >
                                        ( static_cast< ::cppu::OWeakObject* >
                                          ( const_cast< AccessibleEditableTextPara* > (this) )  ) );	// disambiguate hierarchy
    }

    SvxAccessibleTextEditViewAdapter& AccessibleEditableTextPara::GetEditViewForwarder( sal_Bool bCreate ) const SAL_THROW((uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        SvxEditSourceAdapter& rEditSource = GetEditSource();
        SvxAccessibleTextEditViewAdapter* pTextEditViewForwarder = rEditSource.GetEditViewForwarderAdapter( bCreate );

        if( !pTextEditViewForwarder )
        {
            if( bCreate )
                throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Unable to fetch view forwarder, object is defunct")),
                                            uno::Reference< uno::XInterface >
                                            ( static_cast< ::cppu::OWeakObject* >
                                              ( const_cast< AccessibleEditableTextPara* > (this) ) ) );	// disambiguate hierarchy
            else
                throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("No view forwarder, object not in edit mode")),
                                            uno::Reference< uno::XInterface >
                                            ( static_cast< ::cppu::OWeakObject* >
                                              ( const_cast< AccessibleEditableTextPara* > (this) ) ) );	// disambiguate hierarchy
        }

        if( pTextEditViewForwarder->IsValid() )
            return *pTextEditViewForwarder;
        else
        {
            if( bCreate )
                throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("View forwarder is invalid, object is defunct")),
                                            uno::Reference< uno::XInterface >
                                            ( static_cast< ::cppu::OWeakObject* >
                                              ( const_cast< AccessibleEditableTextPara* > (this) )  ) );	// disambiguate hierarchy
            else
                throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("View forwarder is invalid, object not in edit mode")),
                                            uno::Reference< uno::XInterface >
                                            ( static_cast< ::cppu::OWeakObject* >
                                              ( const_cast< AccessibleEditableTextPara* > (this) )  ) );	// disambiguate hierarchy
        }
    }

    sal_Bool AccessibleEditableTextPara::HaveEditView() const
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        SvxEditSource& rEditSource = GetEditSource();
        SvxEditViewForwarder* pViewForwarder = rEditSource.GetEditViewForwarder();

        if( !pViewForwarder )
            return sal_False;

        if( !pViewForwarder->IsValid() )
            return sal_False;

        return sal_True;
    }

    sal_Bool AccessibleEditableTextPara::HaveChildren()
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::HaveChildren: paragraph index value overflow");

        return GetTextForwarder().HaveImageBullet( static_cast< sal_uInt16 >(GetParagraphIndex()) );
    }

    sal_Bool AccessibleEditableTextPara::IsActive() const SAL_THROW((uno::RuntimeException))
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        SvxEditSource& rEditSource = GetEditSource();
        SvxEditViewForwarder* pViewForwarder = rEditSource.GetEditViewForwarder();

        if( !pViewForwarder )
            return sal_False;

        if( pViewForwarder->IsValid() )
            return sal_False;
        else
            return sal_True;
    }

    Rectangle AccessibleEditableTextPara::LogicToPixel( const Rectangle& rRect, const MapMode& rMapMode, SvxViewForwarder& rForwarder )
    {
        // convert to screen coordinates
        return Rectangle( rForwarder.LogicToPixel( rRect.TopLeft(), rMapMode ),
                          rForwarder.LogicToPixel( rRect.BottomRight(), rMapMode ) );
    }

    const Point& AccessibleEditableTextPara::GetEEOffset() const
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return maEEOffset;
    }

    void AccessibleEditableTextPara::SetEEOffset( const Point& rOffset )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        WeakBullet::HardRefType aChild( maImageBullet.get() );
        if( aChild.is() )
            aChild->SetEEOffset(rOffset);

        maEEOffset = rOffset;
    }

    void AccessibleEditableTextPara::FireEvent(const sal_Int16 nEventId, const uno::Any& rNewValue, const uno::Any& rOldValue) const
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        uno::Reference < XAccessibleContext > xThis( const_cast< AccessibleEditableTextPara* > (this)->getAccessibleContext() );

        AccessibleEventObject aEvent(xThis, nEventId, rNewValue, rOldValue);

        // #102261# Call global queue for focus events
        if( nEventId == AccessibleEventId::STATE_CHANGED )
            vcl::unohelper::NotifyAccessibleStateEventGlobally( aEvent );

        // #106234# Delegate to EventNotifier
        if( getNotifierClientId() != -1 )
            ::comphelper::AccessibleEventNotifier::addEvent( getNotifierClientId(),
                                                             aEvent );
    }

    void AccessibleEditableTextPara::GotPropertyEvent( const uno::Any& rNewValue, const sal_Int16 nEventId ) const
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        FireEvent( nEventId, rNewValue );
    }

    void AccessibleEditableTextPara::LostPropertyEvent( const uno::Any& rOldValue, const sal_Int16 nEventId ) const
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        FireEvent( nEventId, uno::Any(), rOldValue );
    }

    bool AccessibleEditableTextPara::HasState( const sal_Int16 nStateId )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::utl::AccessibleStateSetHelper* pStateSet = static_cast< ::utl::AccessibleStateSetHelper*>(mxStateSet.get());
        if( pStateSet != NULL )
            return pStateSet->contains(nStateId) ? true : false;

        return false;
    }

    void AccessibleEditableTextPara::SetState( const sal_Int16 nStateId )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::utl::AccessibleStateSetHelper* pStateSet = static_cast< ::utl::AccessibleStateSetHelper*>(mxStateSet.get());
        if( pStateSet != NULL &&
            !pStateSet->contains(nStateId) )
        {
            pStateSet->AddState( nStateId );
//IAccessibility2 Implementation 2009-----
		// MT: Removed method IsShapeParaFocusable which was introduced with IA2 - basically it was only about figuring out wether or not the window has the focus, should be solved differently
		// if(IsShapeParaFocusable())
            GotPropertyEvent( uno::makeAny( nStateId ), AccessibleEventId::STATE_CHANGED );
//-----IAccessibility2 Implementation 2009
        }
    }

    void AccessibleEditableTextPara::UnSetState( const sal_Int16 nStateId )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::utl::AccessibleStateSetHelper* pStateSet = static_cast< ::utl::AccessibleStateSetHelper*>(mxStateSet.get());
        if( pStateSet != NULL &&
            pStateSet->contains(nStateId) )
        {
            pStateSet->RemoveState( nStateId );
            LostPropertyEvent( uno::makeAny( nStateId ), AccessibleEventId::STATE_CHANGED );
        }
    }

    void AccessibleEditableTextPara::TextChanged()
    {
        ::rtl::OUString aCurrentString( OCommonAccessibleText::getText() );
        uno::Any aDeleted;
        uno::Any aInserted;
        if( OCommonAccessibleText::implInitTextChangedEvent( maLastTextString, aCurrentString,
                                                             aDeleted, aInserted) )
        {
            FireEvent( AccessibleEventId::TEXT_CHANGED, aInserted, aDeleted );
            maLastTextString = aCurrentString;
        }
    }

    sal_Bool AccessibleEditableTextPara::GetAttributeRun( sal_uInt16& nStartIndex, sal_uInt16& nEndIndex, sal_Int32 nIndex )
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        DBG_ASSERT(nIndex >= 0 && nIndex <= USHRT_MAX,
                   "AccessibleEditableTextPara::GetAttributeRun: index value overflow");

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getLocale: paragraph index value overflow");

        return GetTextForwarder().GetAttributeRun( nStartIndex,
                                                   nEndIndex,
                                                   static_cast< sal_uInt16 >(GetParagraphIndex()),
                                                   static_cast< sal_uInt16 >(nIndex) );
    }

    uno::Any SAL_CALL AccessibleEditableTextPara::queryInterface (const uno::Type & rType) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        uno::Any aRet;

        // must provide XAccesibleText by hand, since it comes publicly inherited by XAccessibleEditableText
        if ( rType == ::getCppuType((uno::Reference< XAccessibleText > *)0) )
        {
	//IAccessibility2 Implementation 2009-----
			//	uno::Reference< XAccessibleText > aAccText = this;
            uno::Reference< XAccessibleText > aAccText = static_cast< XAccessibleEditableText * >(this);
	//-----IAccessibility2 Implementation 2009
            aRet <<= aAccText;
        }
        else if ( rType == ::getCppuType((uno::Reference< XAccessibleEditableText > *)0) )
        {
            uno::Reference< XAccessibleEditableText > aAccEditText = this;
            aRet <<= aAccEditText;
        }
	//IAccessibility2 Implementation 2009-----
	else if ( rType == ::getCppuType((uno::Reference< XAccessibleHypertext > *)0) )
        {
            uno::Reference< XAccessibleHypertext > aAccHyperText = this;
            aRet <<= aAccHyperText;
        }
	//-----IAccessibility2 Implementation 2009
        else
        {
            aRet = AccessibleTextParaInterfaceBase::queryInterface(rType);
        }

        return aRet;
    }

	// XAccessible
    uno::Reference< XAccessibleContext > SAL_CALL AccessibleEditableTextPara::getAccessibleContext() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        // We implement the XAccessibleContext interface in the same object
        return uno::Reference< XAccessibleContext > ( this );
    }

	// XAccessibleContext
    sal_Int32 SAL_CALL AccessibleEditableTextPara::getAccessibleChildCount() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        return HaveChildren() ? 1 : 0;
    }

    uno::Reference< XAccessible > SAL_CALL AccessibleEditableTextPara::getAccessibleChild( sal_Int32 i ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        if( !HaveChildren() )
            throw lang::IndexOutOfBoundsException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("No childs available")),
                                                  uno::Reference< uno::XInterface >
                                                  ( static_cast< ::cppu::OWeakObject* > (this) ) );	// static_cast: disambiguate hierarchy

        if( i != 0 )
            throw lang::IndexOutOfBoundsException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Invalid child index")),
                                                  uno::Reference< uno::XInterface >
                                                  ( static_cast< ::cppu::OWeakObject* > (this) ) );	// static_cast: disambiguate hierarchy

        WeakBullet::HardRefType aChild( maImageBullet.get() );

        if( !aChild.is() )
        {
            // there is no hard reference available, create object then
            AccessibleImageBullet* pChild = new AccessibleImageBullet( uno::Reference< XAccessible >( this ) );
            uno::Reference< XAccessible > xChild( static_cast< ::cppu::OWeakObject* > (pChild), uno::UNO_QUERY );

            if( !xChild.is() )
                throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Child creation failed")),
                                            uno::Reference< uno::XInterface >
                                            ( static_cast< ::cppu::OWeakObject* > (this) ) );

            aChild = WeakBullet::HardRefType( xChild, pChild );

            aChild->SetEditSource( &GetEditSource() );
            aChild->SetParagraphIndex( GetParagraphIndex() );
            aChild->SetIndexInParent( i );

            maImageBullet = aChild;
        }

        return aChild.getRef();
    }

    uno::Reference< XAccessible > SAL_CALL AccessibleEditableTextPara::getAccessibleParent() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

#ifdef DBG_UTIL
        if( !mxParent.is() )
            DBG_TRACE( "AccessibleEditableTextPara::getAccessibleParent: no frontend set, did somebody forgot to call AccessibleTextHelper::SetEventSource()?");
#endif

        return mxParent;
    }

    sal_Int32 SAL_CALL AccessibleEditableTextPara::getAccessibleIndexInParent() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return mnIndexInParent;
    }

    sal_Int16 SAL_CALL AccessibleEditableTextPara::getAccessibleRole() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return AccessibleRole::PARAGRAPH;
    }

    ::rtl::OUString SAL_CALL AccessibleEditableTextPara::getAccessibleDescription() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

//        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        return ::rtl::OUString();
    }

    ::rtl::OUString SAL_CALL AccessibleEditableTextPara::getAccessibleName() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

//        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        return ::rtl::OUString();
    }

    uno::Reference< XAccessibleRelationSet > SAL_CALL AccessibleEditableTextPara::getAccessibleRelationSet() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        // --> OD 2006-01-11 #i27138# - provide relations CONTENT_FLOWS_FROM
        // and CONTENT_FLOWS_TO
        if ( mpParaManager )
        {
            utl::AccessibleRelationSetHelper* pAccRelSetHelper =
                                        new utl::AccessibleRelationSetHelper();
            sal_Int32 nMyParaIndex( GetParagraphIndex() );
            // relation CONTENT_FLOWS_FROM
            if ( nMyParaIndex > 0 &&
                 mpParaManager->IsReferencable( nMyParaIndex - 1 ) )
            {
                uno::Sequence<uno::Reference<XInterface> > aSequence(1);
                aSequence[0] =
                    mpParaManager->GetChild( nMyParaIndex - 1 ).first.get().getRef();
                AccessibleRelation aAccRel( AccessibleRelationType::CONTENT_FLOWS_FROM,
                                            aSequence );
                pAccRelSetHelper->AddRelation( aAccRel );
            }

            // relation CONTENT_FLOWS_TO
            if ( (nMyParaIndex + 1) < (sal_Int32)mpParaManager->GetNum() &&
                 mpParaManager->IsReferencable( nMyParaIndex + 1 ) )
            {
                uno::Sequence<uno::Reference<XInterface> > aSequence(1);
                aSequence[0] =
                    mpParaManager->GetChild( nMyParaIndex + 1 ).first.get().getRef();
                AccessibleRelation aAccRel( AccessibleRelationType::CONTENT_FLOWS_TO,
                                            aSequence );
                pAccRelSetHelper->AddRelation( aAccRel );
            }

            return pAccRelSetHelper;
        }
        else
        {
            // no relations, therefore empty
            return uno::Reference< XAccessibleRelationSet >();
        }
        // <--
    }

    uno::Reference< XAccessibleStateSet > SAL_CALL AccessibleEditableTextPara::getAccessibleStateSet() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        // Create a copy of the state set and return it.
        ::utl::AccessibleStateSetHelper* pStateSet = static_cast< ::utl::AccessibleStateSetHelper*>(mxStateSet.get());

        if( !pStateSet )
            return uno::Reference<XAccessibleStateSet>();
//IAccessibility2 Implementation 2009-----
		uno::Reference<XAccessibleStateSet> xParentStates;
		if (getAccessibleParent().is())
		{
			uno::Reference<XAccessibleContext> xParentContext = getAccessibleParent()->getAccessibleContext();
			xParentStates = xParentContext->getAccessibleStateSet();
		}
		if (xParentStates.is() && xParentStates->contains(AccessibleStateType::EDITABLE) )
		{
			pStateSet->AddState(AccessibleStateType::EDITABLE);
		}
//-----IAccessibility2 Implementation 2009
        return uno::Reference<XAccessibleStateSet>( new ::utl::AccessibleStateSetHelper (*pStateSet) );
    }

    lang::Locale SAL_CALL AccessibleEditableTextPara::getLocale() throw (IllegalAccessibleComponentStateException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        return implGetLocale();
    }

    void SAL_CALL AccessibleEditableTextPara::addEventListener( const uno::Reference< XAccessibleEventListener >& xListener ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        if( getNotifierClientId() != -1 )
            ::comphelper::AccessibleEventNotifier::addEventListener( getNotifierClientId(), xListener );
    }

    void SAL_CALL AccessibleEditableTextPara::removeEventListener( const uno::Reference< XAccessibleEventListener >& xListener ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        if( getNotifierClientId() != -1 )
            ::comphelper::AccessibleEventNotifier::removeEventListener( getNotifierClientId(), xListener );
    }

	// XAccessibleComponent
    sal_Bool SAL_CALL AccessibleEditableTextPara::containsPoint( const awt::Point& aTmpPoint ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::contains: index value overflow");

        awt::Rectangle aTmpRect = getBounds();
        Rectangle aRect( Point(aTmpRect.X, aTmpRect.Y), Size(aTmpRect.Width, aTmpRect.Height) );
        Point aPoint( aTmpPoint.X, aTmpPoint.Y );

        return aRect.IsInside( aPoint );
    }

    uno::Reference< XAccessible > SAL_CALL AccessibleEditableTextPara::getAccessibleAtPoint( const awt::Point& _aPoint ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        if( HaveChildren() )
        {
            // #103862# No longer need to make given position relative
            Point aPoint( _aPoint.X, _aPoint.Y );

            // respect EditEngine offset to surrounding shape/cell
            aPoint -= GetEEOffset();

            // convert to EditEngine coordinate system
            SvxTextForwarder& rCacheTF = GetTextForwarder();
            Point aLogPoint( GetViewForwarder().PixelToLogic( aPoint, rCacheTF.GetMapMode() ) );

            EBulletInfo aBulletInfo = rCacheTF.GetBulletInfo( static_cast< sal_uInt16 > (GetParagraphIndex()) );

            if( aBulletInfo.nParagraph != EE_PARA_NOT_FOUND &&
                aBulletInfo.bVisible &&
                aBulletInfo.nType == SVX_NUM_BITMAP )
            {
                Rectangle aRect = aBulletInfo.aBounds;

                if( aRect.IsInside( aLogPoint ) )
                    return getAccessibleChild(0);
            }
        }

        // no children at all, or none at given position
        return uno::Reference< XAccessible >();
    }

    awt::Rectangle SAL_CALL AccessibleEditableTextPara::getBounds() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getBounds: index value overflow");

        SvxTextForwarder& rCacheTF = GetTextForwarder();
        Rectangle aRect = rCacheTF.GetParaBounds( static_cast< sal_uInt16 >( GetParagraphIndex() ) );

        // convert to screen coordinates
        Rectangle aScreenRect = AccessibleEditableTextPara::LogicToPixel( aRect,
                                                                          rCacheTF.GetMapMode(),
                                                                          GetViewForwarder() );

        // offset from shape/cell
        Point aOffset = GetEEOffset();

        return awt::Rectangle( aScreenRect.Left() + aOffset.X(),
                               aScreenRect.Top() + aOffset.Y(),
                               aScreenRect.GetSize().Width(),
                               aScreenRect.GetSize().Height() );
    }

    awt::Point SAL_CALL AccessibleEditableTextPara::getLocation(  ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        awt::Rectangle aRect = getBounds();

        return awt::Point( aRect.X, aRect.Y );
    }

    awt::Point SAL_CALL AccessibleEditableTextPara::getLocationOnScreen(  ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        // relate us to parent
        uno::Reference< XAccessible > xParent = getAccessibleParent();
        if( xParent.is() )
        {
            uno::Reference< XAccessibleComponent > xParentComponent( xParent, uno::UNO_QUERY );
            if( xParentComponent.is() )
            {
                awt::Point aRefPoint = xParentComponent->getLocationOnScreen();
                awt::Point aPoint = getLocation();
                aPoint.X += aRefPoint.X;
                aPoint.Y += aRefPoint.Y;

                return aPoint;
            }
            // --> OD 2009-12-16 #i88070#
            // fallback to parent's <XAccessibleContext> instance
            else
            {
                uno::Reference< XAccessibleContext > xParentContext = xParent->getAccessibleContext();
                if ( xParentContext.is() )
                {
                    uno::Reference< XAccessibleComponent > xParentContextComponent( xParentContext, uno::UNO_QUERY );
                    if( xParentContextComponent.is() )
                    {
                        awt::Point aRefPoint = xParentContextComponent->getLocationOnScreen();
                        awt::Point aPoint = getLocation();
                        aPoint.X += aRefPoint.X;
                        aPoint.Y += aRefPoint.Y;

                        return aPoint;
                    }
                }
            }
            // <--
        }

        throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Cannot access parent")),
                                    uno::Reference< uno::XInterface >
                                    ( static_cast< XAccessible* > (this) ) );	// disambiguate hierarchy
    }

    awt::Size SAL_CALL AccessibleEditableTextPara::getSize(  ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        awt::Rectangle aRect = getBounds();

        return awt::Size( aRect.Width, aRect.Height );
    }

    void SAL_CALL AccessibleEditableTextPara::grabFocus(  ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        // set cursor to this paragraph
        setSelection(0,0);
    }

    sal_Int32 SAL_CALL AccessibleEditableTextPara::getForeground(  ) throw (::com::sun::star::uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        // #104444# Added to XAccessibleComponent interface
		svtools::ColorConfig aColorConfig;
	    sal_uInt32 nColor = aColorConfig.GetColorValue( svtools::FONTCOLOR ).nColor;
        return static_cast<sal_Int32>(nColor);
    }

    sal_Int32 SAL_CALL AccessibleEditableTextPara::getBackground(  ) throw (::com::sun::star::uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        // #104444# Added to XAccessibleComponent interface
        Color aColor( Application::GetSettings().GetStyleSettings().GetWindowColor().GetColor() );

        // the background is transparent
        aColor.SetTransparency( 0xFF);

        return static_cast<sal_Int32>( aColor.GetColor() );
    }

	// XAccessibleText
    sal_Int32 SAL_CALL AccessibleEditableTextPara::getCaretPosition() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        if( !HaveEditView() )
            return -1;

        ESelection aSelection;
        if( GetEditViewForwarder().GetSelection( aSelection ) &&
            GetParagraphIndex() == aSelection.nEndPara )
        {
            // caret is always nEndPara,nEndPos
			//IAccessibility2 Implementation 2009-----
			EBulletInfo aBulletInfo = GetTextForwarder().GetBulletInfo( static_cast< sal_uInt16 >(GetParagraphIndex()) );
			if( aBulletInfo.nParagraph != EE_PARA_NOT_FOUND &&
				aBulletInfo.bVisible && 
				aBulletInfo.nType != SVX_NUM_BITMAP )
			{
				sal_Int32 nBulletLen = aBulletInfo.aText.Len();
				if( aSelection.nEndPos - nBulletLen >= 0 )
					return aSelection.nEndPos - nBulletLen;
			}
			//-----IAccessibility2 Implementation 2009
            return aSelection.nEndPos;
        }

        // not within this paragraph
        return -1;
    }

    sal_Bool SAL_CALL AccessibleEditableTextPara::setCaretPosition( sal_Int32 nIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return setSelection(nIndex, nIndex);
    }

    sal_Unicode SAL_CALL AccessibleEditableTextPara::getCharacter( sal_Int32 nIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getCharacter: index value overflow");

        return OCommonAccessibleText::getCharacter( nIndex );
    }
    //IAccessibility2 Implementation 2009-----
	static uno::Sequence< ::rtl::OUString > getAttributeNames()
	{
		static uno::Sequence< ::rtl::OUString >* pNames = NULL;

		if( pNames == NULL )
		{
			uno::Sequence< ::rtl::OUString >* pSeq = new uno::Sequence< ::rtl::OUString >( 21 );
			::rtl::OUString* pStrings = pSeq->getArray();
			sal_Int32 i = 0;
	#define STR(x) pStrings[i++] = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM(x))
			//STR("CharBackColor");
			STR("CharColor");
	  STR("CharContoured");
	  STR("CharEmphasis");
			STR("CharEscapement");
			STR("CharFontName");
			STR("CharHeight");
			STR("CharPosture");
	  STR("CharShadowed");
			STR("CharStrikeout");
			STR("CharUnderline");		
			STR("CharUnderlineColor");
			STR("CharWeight");			
		        STR("NumberingLevel");
			STR("NumberingRules");
			STR("ParaAdjust");
			STR("ParaBottomMargin");
			STR("ParaFirstLineIndent");
			STR("ParaLeftMargin");
			STR("ParaLineSpacing");
			STR("ParaRightMargin");
			STR("ParaTabStops");
	#undef STR
			DBG_ASSERT( i == pSeq->getLength(), "Please adjust length" );
			if( i != pSeq->getLength() )
				pSeq->realloc( i );
			pNames = pSeq;
		}
		return *pNames;
	}
//-----IAccessibility2 Implementation 2009
	struct IndexCompare
	{
		const PropertyValue* pValues;
		IndexCompare( const PropertyValue* pVals ) : pValues(pVals) {}
		bool operator() ( const sal_Int32& a, const sal_Int32& b ) const
		{
			return (pValues[a].Name < pValues[b].Name) ? true : false;
		}
	};

	String AccessibleEditableTextPara::GetFieldTypeNameAtIndex(sal_Int32 nIndex)
	{
		String strFldType;
        SvxAccessibleTextAdapter& rCacheTF = GetTextForwarder();
		//For field object info
		sal_Int32 nParaIndex = GetParagraphIndex();
		sal_Int32 nAllFieldLen = 0;
		sal_Int32 nField = rCacheTF.GetFieldCount(sal_uInt16(nParaIndex)), nFoundFieldIndex = -1;
		EFieldInfo ree;
		sal_Int32  reeBegin, reeEnd;
		sal_Int32 nFieldType = -1;
		for(sal_uInt16 j = 0; j < nField; j++)
		{
			ree = rCacheTF.GetFieldInfo(sal_uInt16(nParaIndex), j);
			reeBegin  = ree.aPosition.nIndex + nAllFieldLen;
			reeEnd = reeBegin + ree.aCurrentText.Len();
			nAllFieldLen += (ree.aCurrentText.Len() - 1);
			if( reeBegin > nIndex )
			{
				break;
			}
			if(  nIndex >= reeBegin && nIndex < reeEnd )
			{
				nFoundFieldIndex = j;
				break;
			}
		}
		if( nFoundFieldIndex >= 0  )
		{
			// So we get a field, check its type now.
			nFieldType = ree.pFieldItem->GetField()->GetClassId() ;
		}
		switch(nFieldType)
		{
		case SVX_DATEFIELD:
			{
				const SvxDateField* pDateField = static_cast< const SvxDateField* >(ree.pFieldItem->GetField());
				if (pDateField)
				{
					if (pDateField->GetType() == SVXDATETYPE_FIX)
						strFldType = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("date (fixed)"));
					else if (pDateField->GetType() == SVXDATETYPE_VAR)
						strFldType = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("date (variable)"));
				}
			}
			break;
		case SVX_PAGEFIELD:
			strFldType = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("page-number"));
			break;
		//Sym2_8508, support the sheet name & pages fields
		case SVX_PAGESFIELD:
				strFldType = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("page-count"));
			break;
		case SVX_TABLEFIELD:
				strFldType = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("sheet-name"));
			break;
		//End of Sym2_8508
		case SVX_TIMEFIELD:
			strFldType = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("time"));
			break;
		case SVX_EXT_TIMEFIELD:
			{
				const SvxExtTimeField* pTimeField = static_cast< const SvxExtTimeField* >(ree.pFieldItem->GetField());
				if (pTimeField)
				{
					if (pTimeField->GetType() == SVXTIMETYPE_FIX)
						strFldType = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("time (fixed)"));
					else if (pTimeField->GetType() == SVXTIMETYPE_VAR)
						strFldType = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("time (variable)"));
				}
			}
			break;
		case SVX_AUTHORFIELD:
			strFldType = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("author"));
			break;
		case SVX_EXT_FILEFIELD:
		case SVX_FILEFIELD:
			strFldType = ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("file name"));
		default:
			break;
		}
		return strFldType;
	}

    uno::Sequence< beans::PropertyValue > SAL_CALL AccessibleEditableTextPara::getCharacterAttributes( sal_Int32 nIndex, const ::com::sun::star::uno::Sequence< ::rtl::OUString >& rRequestedAttributes ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );
        ::vos::OGuard aGuard( Application::GetSolarMutex() );
	
		//IAccessibility2 Implementation 2009-----
		//Skip the bullet range to ingnore the bullet text 
		SvxTextForwarder& rCacheTF = GetTextForwarder();
		EBulletInfo aBulletInfo = rCacheTF.GetBulletInfo( static_cast< sal_uInt16 >(GetParagraphIndex()) );
		if (aBulletInfo.bVisible)
			nIndex += aBulletInfo.aText.Len();
		if (nIndex != 0 && nIndex >= getCharacterCount())
			nIndex = getCharacterCount()-1;
		//
		if (nIndex != 0)
			CheckIndex(nIndex);	// may throw IndexOutOfBoundsException

		// refactored by Steve Yin
		bool bSupplementalMode = false;
		uno::Sequence< ::rtl::OUString > aPropertyNames = rRequestedAttributes;
		if (aPropertyNames.getLength() == 0)
		{
			bSupplementalMode = true;
			aPropertyNames = getAttributeNames();
		}
        // get default attribues...
        ::comphelper::SequenceAsHashMap aPropHashMap( getDefaultAttributes( aPropertyNames ) );

        // ... and override them with the direct attributes from the specific position
        uno::Sequence< beans::PropertyValue > aRunAttribs( getRunAttributes( nIndex, aPropertyNames ) );
		//-----IAccessibility2 Implementation 2009
        sal_Int32 nRunAttribs = aRunAttribs.getLength();
        const beans::PropertyValue *pRunAttrib = aRunAttribs.getConstArray();
        for (sal_Int32 k = 0;  k < nRunAttribs;  ++k)
        {
            const beans::PropertyValue &rRunAttrib = pRunAttrib[k];
            aPropHashMap[ rRunAttrib.Name ] = rRunAttrib.Value; //!! should not only be the value !!
        }
#ifdef TL_DEBUG
        {
            uno::Sequence< rtl::OUString > aNames(1);
            aNames.getArray()[0] = rtl::OUString::createFromAscii("CharHeight");
            const rtl::OUString *pNames = aNames.getConstArray();
            const uno::Sequence< beans::PropertyValue > aAttribs( getRunAttributes( nIndex, aNames ) );
            const beans::PropertyValue *pAttribs = aAttribs.getConstArray();
            double d1 = -1.0;
            float  f1 = -1.0;
            if (aAttribs.getLength())
            {
                uno::Any aAny( pAttribs[0].Value );
                aAny >>= d1;
                aAny >>= f1;
            }
            int i = 3;
        }
#endif

        // get resulting sequence
        uno::Sequence< beans::PropertyValue > aRes;
        aPropHashMap >> aRes;

        // since SequenceAsHashMap ignores property handles and property state
        // we have to restore the property state here (property handles are
        // of no use to the accessibility API).
        sal_Int32 nRes = aRes.getLength();
        beans::PropertyValue *pRes = aRes.getArray();
        for (sal_Int32 i = 0;  i < nRes;  ++i)
        {
			beans::PropertyValue &rRes = pRes[i];
            sal_Bool bIsDirectVal = sal_False;
            for (sal_Int32 k = 0;  k < nRunAttribs && !bIsDirectVal;  ++k)
            {
                if (rRes.Name == pRunAttrib[k].Name)
                    bIsDirectVal = sal_True;
            }
            rRes.Handle = -1;
            rRes.State  = bIsDirectVal ? PropertyState_DIRECT_VALUE : PropertyState_DEFAULT_VALUE;
        }
		//IAccessibility2 Implementation 2009-----
		if( bSupplementalMode )
		{
			_correctValues( nIndex, aRes );
			// NumberingPrefix		
			nRes = aRes.getLength();
			aRes.realloc( nRes + 1 );
			pRes = aRes.getArray();
			beans::PropertyValue &rRes = pRes[nRes];
			rRes.Name = rtl::OUString::createFromAscii("NumberingPrefix");
			::rtl::OUString numStr;
			if (aBulletInfo.nType != SVX_NUM_CHAR_SPECIAL && aBulletInfo.nType != SVX_NUM_BITMAP)
				numStr = (::rtl::OUString)aBulletInfo.aText;
			rRes.Value <<= numStr;
			rRes.Handle = -1;
			rRes.State = PropertyState_DIRECT_VALUE;
			//-----IAccessibility2 Implementation 2009
			//For field object.
			String strFieldType = GetFieldTypeNameAtIndex(nIndex);
			if (strFieldType.Len() > 0)
			{
				nRes = aRes.getLength();
				aRes.realloc( nRes + 1 );
				pRes = aRes.getArray();
				beans::PropertyValue &rResField = pRes[nRes];
				beans::PropertyValue aFieldType;
				rResField.Name = rtl::OUString::createFromAscii("FieldType");
				rResField.Value <<= rtl::OUString(strFieldType.ToLowerAscii());
				rResField.Handle = -1;
				rResField.State = PropertyState_DIRECT_VALUE;
        }
		//sort property values
		// build sorted index array
		sal_Int32 nLength = aRes.getLength();
		const beans::PropertyValue* pPairs = aRes.getConstArray();
		sal_Int32* pIndices = new sal_Int32[nLength];
		sal_Int32 i = 0;
		for( i = 0; i < nLength; i++ )
			pIndices[i] = i;
		sort( &pIndices[0], &pIndices[nLength], IndexCompare(pPairs) );
		// create sorted sequences accoring to index array
		uno::Sequence<beans::PropertyValue> aNewValues( nLength );
		beans::PropertyValue* pNewValues = aNewValues.getArray();
		for( i = 0; i < nLength; i++ )
		{
			pNewValues[i] = pPairs[pIndices[i]];
		}
		delete[] pIndices;
		//
        return aNewValues;
		}
		return aRes;
    }

    awt::Rectangle SAL_CALL AccessibleEditableTextPara::getCharacterBounds( sal_Int32 nIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getCharacterBounds: index value overflow");

        // #108900# Have position semantics now for nIndex, as
        // one-past-the-end values are legal, too.
        CheckPosition( nIndex );

        SvxTextForwarder& rCacheTF = GetTextForwarder();
        Rectangle aRect = rCacheTF.GetCharBounds( static_cast< sal_uInt16 >( GetParagraphIndex() ), static_cast< sal_uInt16 >( nIndex ) );

        // convert to screen
        Rectangle aScreenRect = AccessibleEditableTextPara::LogicToPixel( aRect,
                                                                          rCacheTF.GetMapMode(),
                                                                          GetViewForwarder() );
        // #109864# offset from parent (paragraph), but in screen
        // coordinates. This makes sure the internal text offset in
        // the outline view forwarder gets cancelled out here
        awt::Rectangle aParaRect( getBounds() );
        aScreenRect.Move( -aParaRect.X, -aParaRect.Y );

        // offset from shape/cell
        Point aOffset = GetEEOffset();

        return awt::Rectangle( aScreenRect.Left() + aOffset.X(),
                               aScreenRect.Top() + aOffset.Y(),
                               aScreenRect.GetSize().Width(),
                               aScreenRect.GetSize().Height() );
    }

    sal_Int32 SAL_CALL AccessibleEditableTextPara::getCharacterCount() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getCharacterCount: index value overflow");

        return OCommonAccessibleText::getCharacterCount();
    }

    sal_Int32 SAL_CALL AccessibleEditableTextPara::getIndexAtPoint( const awt::Point& rPoint ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );
	//IAccessibility2 Implementation 2009-----
	if ((rPoint.X <= 0) && (rPoint.Y <= 0))
		return 0;
	//-----IAccessibility2 Implementation 2009
        sal_uInt16 nPara, nIndex;

        // offset from surrounding cell/shape
        Point aOffset( GetEEOffset() );
        Point aPoint( rPoint.X - aOffset.X(), rPoint.Y - aOffset.Y() );

        // convert to logical coordinates
        SvxTextForwarder& rCacheTF = GetTextForwarder();
        Point aLogPoint( GetViewForwarder().PixelToLogic( aPoint, rCacheTF.GetMapMode() ) );

        // re-offset to parent (paragraph)
        Rectangle aParaRect = rCacheTF.GetParaBounds( static_cast< sal_uInt16 >( GetParagraphIndex() ) );
        aLogPoint.Move( aParaRect.Left(), aParaRect.Top() );

        if( rCacheTF.GetIndexAtPoint( aLogPoint, nPara, nIndex ) &&
            GetParagraphIndex() == nPara )
        {
            // #102259# Double-check if we're _really_ on the given character
            try
            {
                awt::Rectangle aRect1( getCharacterBounds(nIndex) );
                Rectangle aRect2( aRect1.X, aRect1.Y,
                                  aRect1.Width + aRect1.X, aRect1.Height + aRect1.Y );
                if( aRect2.IsInside( Point( rPoint.X, rPoint.Y ) ) )
                    return nIndex;
                else
                    return -1;
            }
            catch( const lang::IndexOutOfBoundsException& )
            {
                // #103927# Don't throw for invalid nIndex values
                return -1;
            }
        }
        else
        {
            // not within our paragraph
            return -1;
        }
    }

    ::rtl::OUString SAL_CALL AccessibleEditableTextPara::getSelectedText() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getSelectedText: index value overflow");

        if( !HaveEditView() )
            return ::rtl::OUString();

        return OCommonAccessibleText::getSelectedText();
    }

    sal_Int32 SAL_CALL AccessibleEditableTextPara::getSelectionStart() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getSelectionStart: index value overflow");

        if( !HaveEditView() )
            return -1;

        return OCommonAccessibleText::getSelectionStart();
    }

    sal_Int32 SAL_CALL AccessibleEditableTextPara::getSelectionEnd() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getSelectionEnd: index value overflow");

        if( !HaveEditView() )
            return -1;

        return OCommonAccessibleText::getSelectionEnd();
    }

    sal_Bool SAL_CALL AccessibleEditableTextPara::setSelection( sal_Int32 nStartIndex, sal_Int32 nEndIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::setSelection: paragraph index value overflow");

        CheckRange(nStartIndex, nEndIndex);

        try
        {
            SvxEditViewForwarder& rCacheVF = GetEditViewForwarder( sal_True );
            return rCacheVF.SetSelection( MakeSelection(nStartIndex, nEndIndex) );
        }
        catch( const uno::RuntimeException& )
        {
            return sal_False;
        }
    }

    ::rtl::OUString SAL_CALL AccessibleEditableTextPara::getText() throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );
        
        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getText: paragraph index value overflow");

        return OCommonAccessibleText::getText();
    }

    ::rtl::OUString SAL_CALL AccessibleEditableTextPara::getTextRange( sal_Int32 nStartIndex, sal_Int32 nEndIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getTextRange: paragraph index value overflow");

        return OCommonAccessibleText::getTextRange(nStartIndex, nEndIndex);
    }
//IAccessibility2 Implementation 2009-----
	void AccessibleEditableTextPara::_correctValues( const sal_Int32 nIndex,
										   uno::Sequence< PropertyValue >& rValues)
	{
		SvxTextForwarder& rCacheTF = GetTextForwarder();
		sal_Int32 nRes = rValues.getLength();
		beans::PropertyValue *pRes = rValues.getArray();
		for (sal_Int32 i = 0;  i < nRes;  ++i)
		{
			beans::PropertyValue &rRes = pRes[i];
			// Char color
			if (rRes.Name.compareTo(::rtl::OUString::createFromAscii("CharColor"))==0)
			{
				uno::Any &anyChar = rRes.Value;
				sal_uInt32 crChar = (sal_uInt32)(anyChar.pReserved);					
				if (COL_AUTO == crChar )
				{
					uno::Reference< ::com::sun::star::accessibility::XAccessibleComponent > xComponent;
					if (mxParent.is())
					{
						xComponent.set(mxParent,uno::UNO_QUERY);
					}
					else
					{
						xComponent.set(m_xAccInfo,uno::UNO_QUERY);
					}
					if (xComponent.is())
					{
						uno::Reference< ::com::sun::star::accessibility::XAccessibleContext > xContext(xComponent,uno::UNO_QUERY);
						if (xContext->getAccessibleRole() == AccessibleRole::SHAPE 
							|| xContext->getAccessibleRole() == AccessibleRole::TABLE_CELL)
						{
							anyChar <<= COL_BLACK;
						}
						else
						{
							Color cr(xComponent->getBackground());
							crChar = cr.IsDark() ? COL_WHITE : COL_BLACK;
							anyChar <<= crChar;
						}
					}
				}
				continue;
			}
			// Underline
			if(rRes.Name.compareTo(::rtl::OUString::createFromAscii("CharUnderline"))==0)
			{	
				/*
				// MT: Implement XAccessibleTextMarkup, mark with TextMarkupType::SPELLCHECK. This way done in SW.
				if (IsCurrentEditorEnableAutoSpell( mxParent ))
				{
					try
					{
						SvxEditViewForwarder& rCacheVF = GetEditViewForwarder( sal_False );
						sal_Bool bWrong = rCacheVF.IsWrongSpelledWordAtPos( GetParagraphIndex(), nIndex );
						if ( bWrong )
						{
							uno::Any &anyUnderLine = pRes[9].Value;
							// MT IA2: Not needed? sal_uInt16 crUnderLine = (sal_uInt16)(anyUnderLine.pReserved);		
							anyUnderLine <<= (sal_uInt16)UNDERLINE_WAVE;
						}
					}
					catch( const uno::RuntimeException& )
					{
					}
				}
				*/
				continue;
			}
			// Underline color && Mis-spell
			if(rRes.Name.compareTo(::rtl::OUString::createFromAscii("CharUnderlineColor"))==0)
			{	
				uno::Any &anyCharUnderLine = rRes.Value;
				sal_uInt32 crCharUnderLine = (sal_uInt32)(anyCharUnderLine.pReserved);					
				if (COL_AUTO == crCharUnderLine )
				{
					uno::Reference< ::com::sun::star::accessibility::XAccessibleComponent > xComponent;
					if (mxParent.is())
					{
						xComponent.set(mxParent,uno::UNO_QUERY);
					}
					else
					{
						xComponent.set(m_xAccInfo,uno::UNO_QUERY);
					}
					if (xComponent.is())
					{
						uno::Reference< ::com::sun::star::accessibility::XAccessibleContext > xContext(xComponent,uno::UNO_QUERY);
						if (xContext->getAccessibleRole() == AccessibleRole::SHAPE 
							|| xContext->getAccessibleRole() == AccessibleRole::TABLE_CELL)
						{
							anyCharUnderLine <<= COL_BLACK;
						}
						else
						{
							Color cr(xComponent->getBackground());
							crCharUnderLine = cr.IsDark() ? COL_WHITE : COL_BLACK;
							anyCharUnderLine <<= crCharUnderLine;
						}
					}
				}
				// MT: Implement XAccessibleTextMarkup, mark with TextMarkupType::SPELLCHECK. This way done in SW.
				/*
				if (IsCurrentEditorEnableAutoSpell( mxParent ))
				{
					try
					{
						SvxEditViewForwarder& rCacheVF = GetEditViewForwarder( sal_False );
						sal_Bool bWrong = rCacheVF.IsWrongSpelledWordAtPos( GetParagraphIndex(), nIndex );
						if ( bWrong )
						{
							uno::Any &anyUnderLineColor = rRes.Value;
							// MT IA2: Not needed? sal_uInt16 crUnderLineColor = (sal_uInt16)(anyUnderLineColor.pReserved);		
							anyUnderLineColor <<= COL_LIGHTRED;
						}
					}
					catch( const uno::RuntimeException& )
					{
					}
				}
				*/				
				continue;
			}
			// NumberingLevel
			if(rRes.Name.compareTo(::rtl::OUString::createFromAscii("NumberingLevel"))==0)
			{				
				const SvxNumBulletItem& rNumBullet = ( SvxNumBulletItem& )rCacheTF.GetParaAttribs(static_cast< sal_uInt16 >(GetParagraphIndex())).Get(EE_PARA_NUMBULLET);
				if(rNumBullet.GetNumRule()->GetLevelCount()==0)
				{
					rRes.Value <<= (sal_Int16)-1;
					rRes.Handle = -1;
					rRes.State = PropertyState_DIRECT_VALUE;
				}
				else
				{
//					SvxAccessibleTextPropertySet aPropSet( &GetEditSource(),
//						ImplGetSvxCharAndParaPropertiesMap() );
					// MT IA2 TODO: Check if this is the correct replacement for ImplGetSvxCharAndParaPropertiesMap
            		SvxAccessibleTextPropertySet aPropSet( &GetEditSource(), ImplGetSvxTextPortionSvxPropertySet() );

					aPropSet.SetSelection( MakeSelection( 0, GetTextLen() ) );
					rRes.Value = aPropSet._getPropertyValue( rRes.Name, mnParagraphIndex ); 
					rRes.State = aPropSet._getPropertyState( rRes.Name, mnParagraphIndex );
					rRes.Handle = -1;
				}
				continue;
			}
			// NumberingRules	
			if(rRes.Name.compareTo(::rtl::OUString::createFromAscii("NumberingRules"))==0)
			{
				SfxItemSet aAttribs = rCacheTF.GetParaAttribs( static_cast< sal_uInt16 >(GetParagraphIndex()) );
				sal_Bool bVis = ((const SfxUInt16Item&)aAttribs.Get( EE_PARA_BULLETSTATE )).GetValue() ? sal_True : sal_False;
				if(bVis)
				{
					rRes.Value <<= (sal_Int16)-1;
					rRes.Handle = -1;
					rRes.State = PropertyState_DIRECT_VALUE;
				}
				else
				{
					// MT IA2 TODO: Check if this is the correct replacement for ImplGetSvxCharAndParaPropertiesMap
            		SvxAccessibleTextPropertySet aPropSet( &GetEditSource(), ImplGetSvxTextPortionSvxPropertySet() );
					aPropSet.SetSelection( MakeSelection( 0, GetTextLen() ) );
					rRes.Value = aPropSet._getPropertyValue( rRes.Name, mnParagraphIndex ); 
					rRes.State = aPropSet._getPropertyState( rRes.Name, mnParagraphIndex );
					rRes.Handle = -1;
				}
				continue;
			}
		}
	}
    sal_Int32 AccessibleEditableTextPara::SkipField(sal_Int32 nIndex, sal_Bool bForward)
    {
		sal_Int32 nParaIndex = GetParagraphIndex();
		SvxAccessibleTextAdapter& rCacheTF = GetTextForwarder();
		sal_Int32 nAllFieldLen = 0;
		sal_Int32 nField = rCacheTF.GetFieldCount(sal_uInt16(nParaIndex)), nFoundFieldIndex = -1;
		EFieldInfo ree;
		sal_Int32  reeBegin=0, reeEnd=0;
		for(sal_uInt16 j = 0; j < nField; j++)
		{
			ree = rCacheTF.GetFieldInfo(sal_uInt16(nParaIndex), j);
			reeBegin  = ree.aPosition.nIndex + nAllFieldLen;
			reeEnd = reeBegin + ree.aCurrentText.Len();
			nAllFieldLen += (ree.aCurrentText.Len() - 1);
			if( reeBegin > nIndex )
			{
				break;
			}
			if(  nIndex >= reeBegin && nIndex < reeEnd )
			{
				if(ree.pFieldItem->GetField()->GetClassId() != SVX_URLFIELD)
				{
					nFoundFieldIndex = j;
					break;
				}
			}
		}
		if( nFoundFieldIndex >= 0  )
		{
			if( bForward ) 
				return reeEnd - 1;
			else 
				return reeBegin;
		}
		return nIndex;
    }
    sal_Bool AccessibleEditableTextPara::ExtendByField( ::com::sun::star::accessibility::TextSegment& Segment )
    {
		sal_Int32 nParaIndex = GetParagraphIndex();
		SvxAccessibleTextAdapter& rCacheTF = GetTextForwarder();
		sal_Int32 nAllFieldLen = 0;
		sal_Int32 nField = rCacheTF.GetFieldCount(sal_uInt16(nParaIndex)), nFoundFieldIndex = -1;
		EFieldInfo ree;
		sal_Int32  reeBegin=0, reeEnd=0;
		for(sal_uInt16 j = 0; j < nField; j++)
		{
			ree = rCacheTF.GetFieldInfo(sal_uInt16(nParaIndex), j);
			reeBegin  = ree.aPosition.nIndex + nAllFieldLen;
			reeEnd = reeBegin + ree.aCurrentText.Len();
			nAllFieldLen += (ree.aCurrentText.Len() - 1);
			if( reeBegin > Segment.SegmentEnd )
			{
				break;
			}
			if(  (Segment.SegmentEnd > reeBegin && Segment.SegmentEnd <= reeEnd) ||
			      (Segment.SegmentStart >= reeBegin && Segment.SegmentStart < reeEnd)  )
			{
				if(ree.pFieldItem->GetField()->GetClassId() != SVX_URLFIELD)
				{
					nFoundFieldIndex = j;
					break;
				}
			}
		}
		sal_Bool bExtend = sal_False;
		if( nFoundFieldIndex >= 0 )
		{
			if( Segment.SegmentEnd < reeEnd ) 
			{
				Segment.SegmentEnd  = reeEnd;
				bExtend = sal_True;
			}
			if( Segment.SegmentStart > reeBegin ) 
			{
				Segment.SegmentStart = reeBegin;
				bExtend = sal_True;
			}
			if( bExtend )
			{
				//Modified by yanjun for sym2_7393
				//If there is a bullet before the field, should add the bullet length into the segment.
				EBulletInfo aBulletInfo = rCacheTF.GetBulletInfo(sal_uInt16(nParaIndex));
				int nBulletLen = aBulletInfo.aText.Len();
				if (nBulletLen > 0)
				{
					Segment.SegmentEnd += nBulletLen;
					if (nFoundFieldIndex > 0)
						Segment.SegmentStart += nBulletLen;
					Segment.SegmentText = GetTextRange(Segment.SegmentStart, Segment.SegmentEnd);
					//After get the correct field name, should restore the offset value which don't contain the bullet.
					Segment.SegmentEnd -= nBulletLen;
					if (nFoundFieldIndex > 0)
						Segment.SegmentStart -= nBulletLen;
				}
				else
					Segment.SegmentText = GetTextRange(Segment.SegmentStart, Segment.SegmentEnd);
				//End
			}
		}
		return bExtend;
    }
//-----IAccessibility2 Implementation 2009
    ::com::sun::star::accessibility::TextSegment SAL_CALL AccessibleEditableTextPara::getTextAtIndex( sal_Int32 nIndex, sal_Int16 aTextType ) throw (::com::sun::star::lang::IndexOutOfBoundsException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getTextAtIndex: paragraph index value overflow");

        ::com::sun::star::accessibility::TextSegment aResult;
        aResult.SegmentStart = -1;
        aResult.SegmentEnd = -1;

        switch( aTextType )
        {
	//IAccessibility2 Implementation 2009-----
		case AccessibleTextType::CHARACTER:
		case AccessibleTextType::WORD:				
		{
			aResult = OCommonAccessibleText::getTextAtIndex( nIndex, aTextType );
			ExtendByField( aResult );
			break;
            	}
	//-----IAccessibility2 Implementation 2009
            // Not yet handled by OCommonAccessibleText. Missing
            // implGetAttributeRunBoundary() method there
            case AccessibleTextType::ATTRIBUTE_RUN:
            {
                const sal_Int32 nTextLen = GetTextForwarder().GetTextLen( static_cast< sal_uInt16 >( GetParagraphIndex() ) );

                if( nIndex == nTextLen )
                {
                    // #i17014# Special-casing one-behind-the-end character
                    aResult.SegmentStart = aResult.SegmentEnd = nTextLen;
                }
                else
                {
                    sal_uInt16 nStartIndex, nEndIndex;
					//For the bullet paragraph, the bullet string is ingnored for IAText::attributes() function.
					SvxTextForwarder&	rCacheTF = GetTextForwarder();
					// MT IA2: Not used? sal_Int32 nBulletLen = 0;
					EBulletInfo aBulletInfo = rCacheTF.GetBulletInfo( static_cast< sal_uInt16 >(GetParagraphIndex()) );
					if (aBulletInfo.bVisible)
						nIndex += aBulletInfo.aText.Len();
					if (nIndex != 0  && nIndex >= getCharacterCount())
						nIndex = getCharacterCount()-1;
					CheckPosition(nIndex);
                    if( GetAttributeRun(nStartIndex, nEndIndex, nIndex) )
                    {
                        aResult.SegmentText = GetTextRange(nStartIndex, nEndIndex);
						if (aBulletInfo.bVisible)
						{
							nStartIndex -= aBulletInfo.aText.Len();
							nEndIndex -= aBulletInfo.aText.Len();
						}
                        aResult.SegmentStart = nStartIndex;
                        aResult.SegmentEnd = nEndIndex;
                    }
		}
                break;
            }
//IAccessibility2 Implementation 2009-----
            case AccessibleTextType::LINE:
            {
                SvxTextForwarder&	rCacheTF = GetTextForwarder();
                sal_Int32			nParaIndex = GetParagraphIndex();
                // MT IA2: Not needed? sal_Int32 nTextLen = rCacheTF.GetTextLen( static_cast< sal_uInt16 >( nParaIndex ) );
                CheckPosition(nIndex);
		if (nIndex != 0  && nIndex == getCharacterCount())
			--nIndex;
                sal_uInt16 nLine, nLineCount=rCacheTF.GetLineCount( static_cast< sal_uInt16 >( nParaIndex ) ); 
                sal_Int32 nCurIndex;
                //the problem is that rCacheTF.GetLineLen() will include the bullet length. But for the bullet line,
                //the text value doesn't contain the bullet characters. all of the bullet and numbering info are exposed
                //by the IAText::attributes(). So here must do special support for bullet line.
                sal_Int32 nBulletLen = 0;
                for( nLine=0, nCurIndex=0; nLine<nLineCount; ++nLine )
                {
                    if (nLine == 0)
                    {
                        EBulletInfo aBulletInfo = rCacheTF.GetBulletInfo( static_cast< sal_uInt16 >(nParaIndex) );
                        if (aBulletInfo.bVisible)
                        {
                            //in bullet or numbering;
                            nBulletLen = aBulletInfo.aText.Len();
                        }
                    }
                    //nCurIndex += rCacheTF.GetLineLen( static_cast< sal_uInt16 >( nParaIndex ), nLine);
                    sal_Int32 nLineLen = rCacheTF.GetLineLen( static_cast< sal_uInt16 >( nParaIndex ), nLine);
                    if (nLine == 0)
                        nCurIndex += nLineLen - nBulletLen;
                    else 
                        nCurIndex += nLineLen;
                    if( nCurIndex > nIndex )
                    {
                        if (nLine ==0)
                        {
                            //aResult.SegmentStart = nCurIndex - rCacheTF.GetLineLen(static_cast< sal_uInt16 >( nParaIndex ), nLine);
                            aResult.SegmentStart = 0;
                            aResult.SegmentEnd = nCurIndex;
                            //aResult.SegmentText = GetTextRange( aResult.SegmentStart, aResult.SegmentEnd );
                            aResult.SegmentText = GetTextRange( aResult.SegmentStart, aResult.SegmentEnd + nBulletLen);
                            break;
                        }
                        else
                        {
                            //aResult.SegmentStart = nCurIndex - rCacheTF.GetLineLen(static_cast< sal_uInt16 >( nParaIndex ), nLine);
                            aResult.SegmentStart = nCurIndex - nLineLen;
                            aResult.SegmentEnd = nCurIndex;
                            //aResult.SegmentText = GetTextRange( aResult.SegmentStart, aResult.SegmentEnd );
                            aResult.SegmentText = GetTextRange( aResult.SegmentStart + nBulletLen, aResult.SegmentEnd + nBulletLen);
                            break;
                        }
                    }
                }
                break;
            }
//-----IAccessibility2 Implementation 2009
            default:
                aResult = OCommonAccessibleText::getTextAtIndex( nIndex, aTextType );
                break;
        } /* end of switch( aTextType ) */

        return aResult;
    }

    ::com::sun::star::accessibility::TextSegment SAL_CALL AccessibleEditableTextPara::getTextBeforeIndex( sal_Int32 nIndex, sal_Int16 aTextType ) throw (::com::sun::star::lang::IndexOutOfBoundsException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getTextBeforeIndex: paragraph index value overflow");

        ::com::sun::star::accessibility::TextSegment aResult;
        aResult.SegmentStart = -1;
        aResult.SegmentEnd = -1;
//IAccessibility2 Implementation 2009-----
		i18n::Boundary aBoundary;
//-----IAccessibility2 Implementation 2009
        switch( aTextType )
        {
            // Not yet handled by OCommonAccessibleText. Missing
            // implGetAttributeRunBoundary() method there
            case AccessibleTextType::ATTRIBUTE_RUN:
            {
                const sal_Int32 nTextLen = GetTextForwarder().GetTextLen( static_cast< sal_uInt16 >( GetParagraphIndex() ) );
                sal_uInt16 nStartIndex, nEndIndex;

                if( nIndex == nTextLen )
                {
                    // #i17014# Special-casing one-behind-the-end character
                    if( nIndex > 0 &&
                        GetAttributeRun(nStartIndex, nEndIndex, nIndex-1) )
                    {
                        aResult.SegmentText = GetTextRange(nStartIndex, nEndIndex);
                        aResult.SegmentStart = nStartIndex;
                        aResult.SegmentEnd = nEndIndex;
                    }
                }
                else
                {
                    if( GetAttributeRun(nStartIndex, nEndIndex, nIndex) )
                    {
                        // already at the left border? If not, query
                        // one index further left
                        if( nStartIndex > 0 &&
                            GetAttributeRun(nStartIndex, nEndIndex, nStartIndex-1) )
                        {
                            aResult.SegmentText = GetTextRange(nStartIndex, nEndIndex);
                            aResult.SegmentStart = nStartIndex;
                            aResult.SegmentEnd = nEndIndex;
                        }
                    }
                }
                break;
            }
	    //IAccessibility2 Implementation 2009-----
            case AccessibleTextType::LINE:
            {
                SvxTextForwarder&	rCacheTF = GetTextForwarder();
                sal_Int32			nParaIndex = GetParagraphIndex();
                // MT IA2 not needed? sal_Int32 nTextLen = rCacheTF.GetTextLen( static_cast< sal_uInt16 >( nParaIndex ) );

                CheckPosition(nIndex);

                sal_uInt16 nLine, nLineCount=rCacheTF.GetLineCount( static_cast< sal_uInt16 >( nParaIndex ) ); 
                //the problem is that rCacheTF.GetLineLen() will include the bullet length. But for the bullet line,
                //the text value doesn't contain the bullet characters. all of the bullet and numbering info are exposed
                //by the IAText::attributes(). So here must do special support for bullet line.
                sal_Int32 nCurIndex=0, nLastIndex=0, nCurLineLen=0; 
                sal_Int32 nLastLineLen = 0, nBulletLen = 0;;
                // get the line before the line the index points into
                for( nLine=0, nCurIndex=0, nLastIndex=0; nLine<nLineCount; ++nLine )
                {
                    nLastIndex = nCurIndex;
                    if (nLine == 0)
                    {
                        EBulletInfo aBulletInfo = rCacheTF.GetBulletInfo( static_cast< sal_uInt16 >(nParaIndex) );
                        if (aBulletInfo.bVisible)
                        {
                            //in bullet or numbering;
                            nBulletLen = aBulletInfo.aText.Len();
                        }
                    }
                    if (nLine == 1)
                        nLastLineLen = nCurLineLen - nBulletLen;
                    else
                        nLastLineLen = nCurLineLen;
                    nCurLineLen = rCacheTF.GetLineLen(static_cast< sal_uInt16 >( nParaIndex ), nLine);
                    //nCurIndex += nCurLineLen;
                    if (nLine == 0)
                        nCurIndex += nCurLineLen - nBulletLen;
                    else 
                        nCurIndex += nCurLineLen;
                    
                    //if( nCurIndex > nIndex &&
                    //nLastIndex > nCurLineLen )
                    if (nCurIndex > nIndex)
                    {
                        if (nLine == 0)
                        {
                            break;
                        }
                        else if (nLine == 1)
                        {
                            aResult.SegmentStart = 0;
                            aResult.SegmentEnd = static_cast< sal_uInt16 >( nLastIndex );
                            aResult.SegmentText = GetTextRange( aResult.SegmentStart, aResult.SegmentEnd + nBulletLen);
                            break;
                        }
                        else
                        {
                            //aResult.SegmentStart = nLastIndex - nCurLineLen;
                            aResult.SegmentStart = nLastIndex - nLastLineLen;
                            aResult.SegmentEnd = static_cast< sal_uInt16 >( nLastIndex );
                            aResult.SegmentText = GetTextRange( aResult.SegmentStart + nBulletLen, aResult.SegmentEnd + nBulletLen);
                            break;
                        }
                    }                
                }

                break;
            }
			case AccessibleTextType::WORD:
			{
				nIndex = SkipField( nIndex, sal_False);
				::rtl::OUString sText( implGetText() );
				sal_Int32 nLength = sText.getLength();

				// get word at index
				implGetWordBoundary( aBoundary, nIndex );


				//sal_Int32 curWordStart = aBoundary.startPos;
				//sal_Int32 preWordStart = curWordStart;
				sal_Int32 curWordStart , preWordStart;
				if( aBoundary.startPos == -1 || aBoundary.startPos > nIndex)
					curWordStart = preWordStart = nIndex;
				else
					curWordStart = preWordStart = aBoundary.startPos;
				
				// get previous word
				
				sal_Bool bWord = sal_False;
				
				//while ( preWordStart > 0 && aBoundary.startPos == curWordStart)
				while ( (preWordStart >= 0 && !bWord ) || ( aBoundary.endPos > curWordStart ) )
					{
					preWordStart--;
					bWord = implGetWordBoundary( aBoundary, preWordStart );	
				}
				if ( bWord && implIsValidBoundary( aBoundary, nLength ) )
				{
					aResult.SegmentText = sText.copy( aBoundary.startPos, aBoundary.endPos - aBoundary.startPos );
					aResult.SegmentStart = aBoundary.startPos;
					aResult.SegmentEnd = aBoundary.endPos;
					ExtendByField( aResult );					
				}
			}
			break;
			case AccessibleTextType::CHARACTER:
			{
				nIndex = SkipField( nIndex, sal_False);
				aResult = OCommonAccessibleText::getTextBeforeIndex( nIndex, aTextType );
				ExtendByField( aResult );
				break;
			}
			//-----IAccessibility2 Implementation 2009
            default:
                aResult = OCommonAccessibleText::getTextBeforeIndex( nIndex, aTextType );
                break;
        } /* end of switch( aTextType ) */

        return aResult;
    }

    ::com::sun::star::accessibility::TextSegment SAL_CALL AccessibleEditableTextPara::getTextBehindIndex( sal_Int32 nIndex, sal_Int16 aTextType ) throw (::com::sun::star::lang::IndexOutOfBoundsException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getTextBehindIndex: paragraph index value overflow");

        ::com::sun::star::accessibility::TextSegment aResult;
        aResult.SegmentStart = -1;
        aResult.SegmentEnd = -1;
//IAccessibility2 Implementation 2009-----
		i18n::Boundary aBoundary;
//-----IAccessibility2 Implementation 2009
        switch( aTextType )
        {
            case AccessibleTextType::ATTRIBUTE_RUN:
            {
                sal_uInt16 nStartIndex, nEndIndex;

                if( GetAttributeRun(nStartIndex, nEndIndex, nIndex) )
                {
                    // already at the right border?
                    if( nEndIndex < GetTextLen() )
                    {
                        if( GetAttributeRun(nStartIndex, nEndIndex, nEndIndex) )
                        {
                            aResult.SegmentText = GetTextRange(nStartIndex, nEndIndex);
                            aResult.SegmentStart = nStartIndex;
                            aResult.SegmentEnd = nEndIndex;
                        }
                    }
                }
                break;
            }

//IAccessibility2 Implementation 2009-----
            case AccessibleTextType::LINE:
            {
                SvxTextForwarder&	rCacheTF = GetTextForwarder();
                sal_Int32			nParaIndex = GetParagraphIndex();
                // MT IA2 not needed? sal_Int32 nTextLen = rCacheTF.GetTextLen( static_cast< sal_uInt16 >( nParaIndex ) );

                CheckPosition(nIndex);

                sal_uInt16 nLine, nLineCount=rCacheTF.GetLineCount( static_cast< sal_uInt16 >( nParaIndex ) ); 
                sal_Int32 nCurIndex; 
                //the problem is that rCacheTF.GetLineLen() will include the bullet length. But for the bullet line,
                //the text value doesn't contain the bullet characters. all of the bullet and numbering info are exposed
                //by the IAText::attributes(). So here must do special support for bullet line.
                sal_Int32 nBulletLen = 0;
                // get the line after the line the index points into
                for( nLine=0, nCurIndex=0; nLine<nLineCount; ++nLine )
                {
                    if (nLine == 0)
                    {
                        EBulletInfo aBulletInfo = rCacheTF.GetBulletInfo( static_cast< sal_uInt16 >(nParaIndex) );
                        if (aBulletInfo.bVisible)
                        {
                            //in bullet or numbering;
                            nBulletLen = aBulletInfo.aText.Len();
                        }
                    }
                    //nCurIndex += rCacheTF.GetLineLen(static_cast< sal_uInt16 >( nParaIndex ), nLine);
                    sal_Int32 nLineLen = rCacheTF.GetLineLen( static_cast< sal_uInt16 >( nParaIndex ), nLine);
                    
                    if (nLine == 0)
                        nCurIndex += nLineLen - nBulletLen;
                    else 
                        nCurIndex += nLineLen;
                    
                    if( nCurIndex > nIndex &&
                        nLine < nLineCount-1 )
                    {
                        aResult.SegmentStart = nCurIndex;
                        aResult.SegmentEnd = nCurIndex + rCacheTF.GetLineLen(static_cast< sal_uInt16 >( nParaIndex ), nLine+1);
                        aResult.SegmentText = GetTextRange( aResult.SegmentStart + nBulletLen, aResult.SegmentEnd + nBulletLen);
                        break;
                    }                
                }

                break;
            }
			case AccessibleTextType::WORD:
			{
				nIndex = SkipField( nIndex, sal_True);
				::rtl::OUString sText( implGetText() );
				sal_Int32 nLength = sText.getLength();

				// get word at index
				sal_Bool bWord = implGetWordBoundary( aBoundary, nIndex );

				// real current world
				sal_Int32 nextWord = nIndex;
				//if( nIndex >= aBoundary.startPos && nIndex <= aBoundary.endPos )
				if( nIndex <= aBoundary.endPos )
				{		
					nextWord = 	aBoundary.endPos;
					if( sText.getStr()[nextWord] == sal_Unicode(' ') ) nextWord++;
					bWord = implGetWordBoundary( aBoundary, nextWord );
				}
				
				if ( bWord && implIsValidBoundary( aBoundary, nLength ) )
				{
					aResult.SegmentText = sText.copy( aBoundary.startPos, aBoundary.endPos - aBoundary.startPos );
					aResult.SegmentStart = aBoundary.startPos;
					aResult.SegmentEnd = aBoundary.endPos;

					// If the end position of aBoundary is inside a field, extend the result to the end of the field

					ExtendByField( aResult );
				}
			}
			break;

			case AccessibleTextType::CHARACTER:
			{
				nIndex = SkipField( nIndex, sal_True);
				aResult = OCommonAccessibleText::getTextBehindIndex( nIndex, aTextType );
				ExtendByField( aResult );
				break;
			}
			//-----IAccessibility2 Implementation 2009
            default:
                aResult = OCommonAccessibleText::getTextBehindIndex( nIndex, aTextType );
                break;
        } /* end of switch( aTextType ) */

        return aResult;
    }

    sal_Bool SAL_CALL AccessibleEditableTextPara::copyText( sal_Int32 nStartIndex, sal_Int32 nEndIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        try
        {
            SvxEditViewForwarder& rCacheVF = GetEditViewForwarder( sal_True );
            #if OSL_DEBUG_LEVEL > 0
            SvxAccessibleTextAdapter& rCacheTF = GetTextForwarder();    // MUST be after GetEditViewForwarder(), see method docs
            (void)rCacheTF;
            #else
            GetTextForwarder();                                         // MUST be after GetEditViewForwarder(), see method docs
            #endif

            sal_Bool aRetVal;

            DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                       "AccessibleEditableTextPara::copyText: index value overflow");

            CheckRange(nStartIndex, nEndIndex);

            //Because bullet may occupy one or more characters, the TextAdapter will include bullet to calculate the selection. Add offset to handle bullet
            sal_Int32 nBulletLen = 0;
            EBulletInfo aBulletInfo = GetTextForwarder().GetBulletInfo( static_cast< sal_uInt16 >(GetParagraphIndex()) );
            if( aBulletInfo.nParagraph != EE_PARA_NOT_FOUND && aBulletInfo.bVisible )
                        nBulletLen = aBulletInfo.aText.Len();
            // save current selection
            ESelection aOldSelection;

            rCacheVF.GetSelection( aOldSelection );
            //rCacheVF.SetSelection( MakeSelection(nStartIndex, nEndIndex) );
            rCacheVF.SetSelection( MakeSelection(nStartIndex + nBulletLen, nEndIndex + nBulletLen) );
            aRetVal = rCacheVF.Copy();
            rCacheVF.SetSelection( aOldSelection ); // restore

            return aRetVal;
        }
        catch( const uno::RuntimeException& )
        {
            return sal_False;
        }
    }

	// XAccessibleEditableText
    sal_Bool SAL_CALL AccessibleEditableTextPara::cutText( sal_Int32 nStartIndex, sal_Int32 nEndIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        try
        {
            SvxEditViewForwarder& rCacheVF = GetEditViewForwarder( sal_True );
            SvxAccessibleTextAdapter& rCacheTF = GetTextForwarder();	// MUST be after GetEditViewForwarder(), see method docs

            DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                       "AccessibleEditableTextPara::cutText: index value overflow");

            CheckRange(nStartIndex, nEndIndex);

            // Because bullet may occupy one or more characters, the TextAdapter will include bullet to calculate the selection. Add offset to handle bullet
            sal_Int32 nBulletLen = 0;
            EBulletInfo aBulletInfo = GetTextForwarder().GetBulletInfo( static_cast< sal_uInt16 >(GetParagraphIndex()) );
            if( aBulletInfo.nParagraph != EE_PARA_NOT_FOUND && aBulletInfo.bVisible )
                        nBulletLen = aBulletInfo.aText.Len();
            ESelection aSelection = MakeSelection (nStartIndex + nBulletLen, nEndIndex + nBulletLen);
            //if( !rCacheTF.IsEditable( MakeSelection(nStartIndex, nEndIndex) ) )
            if( !rCacheTF.IsEditable( aSelection ) )
                return sal_False; // non-editable area selected

            // don't save selection, might become invalid after cut!
            //rCacheVF.SetSelection( MakeSelection(nStartIndex, nEndIndex) );
            rCacheVF.SetSelection( aSelection );

            return rCacheVF.Cut();
        }
        catch( const uno::RuntimeException& )
        {
            return sal_False;
        }
    }

    sal_Bool SAL_CALL AccessibleEditableTextPara::pasteText( sal_Int32 nIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        try
        {
            SvxEditViewForwarder& rCacheVF = GetEditViewForwarder( sal_True );
            SvxAccessibleTextAdapter& rCacheTF = GetTextForwarder();	// MUST be after GetEditViewForwarder(), see method docs

            DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                       "AccessibleEditableTextPara::pasteText: index value overflow");

            CheckPosition(nIndex);

            // Because bullet may occupy one or more characters, the TextAdapter will include bullet to calculate the selection. Add offset to handle bullet
            sal_Int32 nBulletLen = 0;
            EBulletInfo aBulletInfo = GetTextForwarder().GetBulletInfo( static_cast< sal_uInt16 >(GetParagraphIndex()) );
            if( aBulletInfo.nParagraph != EE_PARA_NOT_FOUND && aBulletInfo.bVisible )
                        nBulletLen = aBulletInfo.aText.Len();
            //if( !rCacheTF.IsEditable( MakeSelection(nIndex) ) )
            if( !rCacheTF.IsEditable( MakeSelection(nIndex + nBulletLen) ) )
                return sal_False; // non-editable area selected

            // #104400# set empty selection (=> cursor) to given index
            //rCacheVF.SetSelection( MakeCursor(nIndex) );
            rCacheVF.SetSelection( MakeCursor(nIndex + nBulletLen) );

            return rCacheVF.Paste();
        }
        catch( const uno::RuntimeException& )
        {
            return sal_False;
        }
    }

    sal_Bool SAL_CALL AccessibleEditableTextPara::deleteText( sal_Int32 nStartIndex, sal_Int32 nEndIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        try
        {
            // #102710# Request edit view when doing changes
            // AccessibleEmptyEditSource relies on this behaviour
            GetEditViewForwarder( sal_True );
            SvxAccessibleTextAdapter& rCacheTF = GetTextForwarder();	// MUST be after GetEditViewForwarder(), see method docs

            DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                       "AccessibleEditableTextPara::deleteText: index value overflow");

            CheckRange(nStartIndex, nEndIndex);

            // Because bullet may occupy one or more characters, the TextAdapter will include bullet to calculate the selection. Add offset to handle bullet
            sal_Int32 nBulletLen = 0;
            EBulletInfo aBulletInfo = GetTextForwarder().GetBulletInfo( static_cast< sal_uInt16 >(GetParagraphIndex()) );
            if( aBulletInfo.nParagraph != EE_PARA_NOT_FOUND && aBulletInfo.bVisible )
                        nBulletLen = aBulletInfo.aText.Len();
            ESelection aSelection = MakeSelection (nStartIndex + nBulletLen, nEndIndex + nBulletLen);

            //if( !rCacheTF.IsEditable( MakeSelection(nStartIndex, nEndIndex) ) )
            if( !rCacheTF.IsEditable( aSelection ) )
                return sal_False; // non-editable area selected

            //sal_Bool bRet = rCacheTF.Delete( MakeSelection(nStartIndex, nEndIndex) );
            sal_Bool bRet = rCacheTF.Delete( aSelection );

            GetEditSource().UpdateData();

            return bRet;
        }
        catch( const uno::RuntimeException& )
        {
            return sal_False;
        }
    }

    sal_Bool SAL_CALL AccessibleEditableTextPara::insertText( const ::rtl::OUString& sText, sal_Int32 nIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        try
        {
            // #102710# Request edit view when doing changes
            // AccessibleEmptyEditSource relies on this behaviour
            GetEditViewForwarder( sal_True );
            SvxAccessibleTextAdapter& rCacheTF = GetTextForwarder();	// MUST be after GetEditViewForwarder(), see method docs

            DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                       "AccessibleEditableTextPara::insertText: index value overflow");

            CheckPosition(nIndex);

            // Because bullet may occupy one or more characters, the TextAdapter will include bullet to calculate the selection. Add offset to handle bullet
            sal_Int32 nBulletLen = 0;
            EBulletInfo aBulletInfo = GetTextForwarder().GetBulletInfo( static_cast< sal_uInt16 >(GetParagraphIndex()) );
            if( aBulletInfo.nParagraph != EE_PARA_NOT_FOUND && aBulletInfo.bVisible )
                        nBulletLen = aBulletInfo.aText.Len();

            //if( !rCacheTF.IsEditable( MakeSelection(nIndex) ) )
            if( !rCacheTF.IsEditable( MakeSelection(nIndex + nBulletLen) ) )
                return sal_False; // non-editable area selected

            // #104400# insert given text at empty selection (=> cursor)
            //sal_Bool bRet = rCacheTF.InsertText( sText, MakeCursor(nIndex) );
            sal_Bool bRet = rCacheTF.InsertText( sText, MakeCursor(nIndex + nBulletLen) );

            rCacheTF.QuickFormatDoc();
            GetEditSource().UpdateData();

            return bRet;
        }
        catch( const uno::RuntimeException& )
        {
            return sal_False;
        }
    }

    sal_Bool SAL_CALL AccessibleEditableTextPara::replaceText( sal_Int32 nStartIndex, sal_Int32 nEndIndex, const ::rtl::OUString& sReplacement ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        try
        {
            // #102710# Request edit view when doing changes
            // AccessibleEmptyEditSource relies on this behaviour
            GetEditViewForwarder( sal_True );
            SvxAccessibleTextAdapter& rCacheTF = GetTextForwarder();	// MUST be after GetEditViewForwarder(), see method docs

            DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                       "AccessibleEditableTextPara::replaceText: index value overflow");

            CheckRange(nStartIndex, nEndIndex);

            // Because bullet may occupy one or more characters, the TextAdapter will include bullet to calculate the selection. Add offset to handle bullet
            sal_Int32 nBulletLen = 0;
            EBulletInfo aBulletInfo = GetTextForwarder().GetBulletInfo( static_cast< sal_uInt16 >(GetParagraphIndex()) );
            if( aBulletInfo.nParagraph != EE_PARA_NOT_FOUND && aBulletInfo.bVisible )
                        nBulletLen = aBulletInfo.aText.Len();
            ESelection aSelection = MakeSelection (nStartIndex + nBulletLen, nEndIndex + nBulletLen);

            //if( !rCacheTF.IsEditable( MakeSelection(nStartIndex, nEndIndex) ) )
            if( !rCacheTF.IsEditable( aSelection ) )
                return sal_False; // non-editable area selected

            // insert given text into given range => replace
            //sal_Bool bRet = rCacheTF.InsertText( sReplacement, MakeSelection(nStartIndex, nEndIndex) );
            sal_Bool bRet = rCacheTF.InsertText( sReplacement, aSelection );

            rCacheTF.QuickFormatDoc();
            GetEditSource().UpdateData();

            return bRet;
        }
        catch( const uno::RuntimeException& )
        {
            return sal_False;
        }
    }

    sal_Bool SAL_CALL AccessibleEditableTextPara::setAttributes( sal_Int32 nStartIndex, sal_Int32 nEndIndex, const uno::Sequence< beans::PropertyValue >& aAttributeSet ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        try
        {
            // #102710# Request edit view when doing changes
            // AccessibleEmptyEditSource relies on this behaviour
            GetEditViewForwarder( sal_True );
            SvxAccessibleTextAdapter& rCacheTF = GetTextForwarder();	// MUST be after GetEditViewForwarder(), see method docs
            sal_uInt16 nPara = static_cast< sal_uInt16 >( GetParagraphIndex() );

            DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                       "AccessibleEditableTextPara::setAttributes: index value overflow");

            CheckRange(nStartIndex, nEndIndex);

            if( !rCacheTF.IsEditable( MakeSelection(nStartIndex, nEndIndex) ) )
                return sal_False; // non-editable area selected

            // do the indices span the whole paragraph? Then use the outliner map
            // TODO: hold it as a member?
            SvxAccessibleTextPropertySet aPropSet( &GetEditSource(),
                                                   0 == nStartIndex &&
                                                   rCacheTF.GetTextLen(nPara) == nEndIndex ?
                                                   ImplGetSvxUnoOutlinerTextCursorSvxPropertySet() :
                                                   ImplGetSvxTextPortionSvxPropertySet() );

            aPropSet.SetSelection( MakeSelection(nStartIndex, nEndIndex) );

            // convert from PropertyValue to Any
            sal_Int32 i, nLength( aAttributeSet.getLength() );
            const beans::PropertyValue*	pPropArray = aAttributeSet.getConstArray();
            for(i=0; i<nLength; ++i)
            {
                try
                {
                    aPropSet.setPropertyValue(pPropArray->Name, pPropArray->Value);
                }
                catch( const uno::Exception& )
                {
                    DBG_ERROR("AccessibleEditableTextPara::setAttributes exception in setPropertyValue");
                }

                ++pPropArray;
            }

            rCacheTF.QuickFormatDoc();
            GetEditSource().UpdateData();

            return sal_True;
        }
        catch( const uno::RuntimeException& )
        {
            return sal_False;
        }
    }

    sal_Bool SAL_CALL AccessibleEditableTextPara::setText( const ::rtl::OUString& sText ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        return replaceText(0, getCharacterCount(), sText);
    }

    // XAccessibleTextAttributes
    uno::Sequence< beans::PropertyValue > SAL_CALL AccessibleEditableTextPara::getDefaultAttributes(
            const uno::Sequence< ::rtl::OUString >& rRequestedAttributes )
        throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );
        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        #if OSL_DEBUG_LEVEL > 0
        SvxAccessibleTextAdapter& rCacheTF =
        #endif
            GetTextForwarder();

        #if OSL_DEBUG_LEVEL > 0
        (void)rCacheTF;
        #endif

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getCharacterAttributes: index value overflow");

        // get XPropertySetInfo for paragraph attributes and
        // character attributes that span all the paragraphs text.
        SvxAccessibleTextPropertySet aPropSet( &GetEditSource(),
                ImplGetSvxCharAndParaPropertiesSet() );
        aPropSet.SetSelection( MakeSelection( 0, GetTextLen() ) );
        uno::Reference< beans::XPropertySetInfo > xPropSetInfo = aPropSet.getPropertySetInfo();
        if (!xPropSetInfo.is())
            throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Cannot query XPropertySetInfo")),
                        uno::Reference< uno::XInterface >
                        ( static_cast< XAccessible* > (this) ) );   // disambiguate hierarchy

        // build sequence of available properties to check
        sal_Int32 nLenReqAttr = rRequestedAttributes.getLength();
        uno::Sequence< beans::Property > aProperties;
        if (nLenReqAttr)
        {
            const rtl::OUString *pRequestedAttributes = rRequestedAttributes.getConstArray();

            aProperties.realloc( nLenReqAttr );
            beans::Property *pProperties = aProperties.getArray();
            sal_Int32 nCurLen = 0;
            for (sal_Int32 i = 0;  i < nLenReqAttr;  ++i)
            {
                beans::Property aProp;
                try
                {
                    aProp = xPropSetInfo->getPropertyByName( pRequestedAttributes[i] );
                }
                catch (beans::UnknownPropertyException &)
                {
                    continue;
                }
                pProperties[ nCurLen++ ] = aProp;
            }
            aProperties.realloc( nCurLen );
        }
        else
            aProperties = xPropSetInfo->getProperties();

        sal_Int32 nLength = aProperties.getLength();
        const beans::Property *pProperties = aProperties.getConstArray();

        // build resulting sequence
        uno::Sequence< beans::PropertyValue > aOutSequence( nLength );
        beans::PropertyValue* pOutSequence = aOutSequence.getArray();
        sal_Int32 nOutLen = 0;
        for (sal_Int32 i = 0;  i < nLength;  ++i)
        {
            // calling implementation functions:
            // _getPropertyState and _getPropertyValue (see below) to provide
            // the proper paragraph number when retrieving paragraph attributes
            PropertyState eState = aPropSet._getPropertyState( pProperties->Name, mnParagraphIndex );
            if ( eState == PropertyState_AMBIGUOUS_VALUE )
            {
                OSL_ENSURE( false, "ambiguous property value encountered" );
            }

            //if (eState == PropertyState_DIRECT_VALUE)
            // per definition all paragraph properties and all character
            // properties spanning the whole paragraph should be returned
            // and declared as default value
            {
                pOutSequence->Name      = pProperties->Name;
                pOutSequence->Handle    = pProperties->Handle;
                pOutSequence->Value     = aPropSet._getPropertyValue( pProperties->Name, mnParagraphIndex );
                pOutSequence->State     = PropertyState_DEFAULT_VALUE;

                ++pOutSequence;
                ++nOutLen;
            }
            ++pProperties;
        }
        aOutSequence.realloc( nOutLen );

        return aOutSequence;
    }


    uno::Sequence< beans::PropertyValue > SAL_CALL AccessibleEditableTextPara::getRunAttributes(
            sal_Int32 nIndex,
            const uno::Sequence< ::rtl::OUString >& rRequestedAttributes )
        throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::vos::OGuard aGuard( Application::GetSolarMutex() );

        #if OSL_DEBUG_LEVEL > 0
        SvxAccessibleTextAdapter& rCacheTF =
        #endif
            GetTextForwarder();

        #if OSL_DEBUG_LEVEL > 0
        (void)rCacheTF;
        #endif

        DBG_ASSERT(GetParagraphIndex() >= 0 && GetParagraphIndex() <= USHRT_MAX,
                   "AccessibleEditableTextPara::getCharacterAttributes: index value overflow");

		if( getCharacterCount() > 0 )
			CheckIndex(nIndex);
		else
			CheckPosition(nIndex);

        SvxAccessibleTextPropertySet aPropSet( &GetEditSource(),
                                               ImplGetSvxCharAndParaPropertiesSet() );
        aPropSet.SetSelection( MakeSelection( nIndex ) );
        uno::Reference< beans::XPropertySetInfo > xPropSetInfo = aPropSet.getPropertySetInfo();
        if (!xPropSetInfo.is())
            throw uno::RuntimeException(::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("Cannot query XPropertySetInfo")),
                                        uno::Reference< uno::XInterface >
                                        ( static_cast< XAccessible* > (this) ) );   // disambiguate hierarchy

        // build sequence of available properties to check
        sal_Int32 nLenReqAttr = rRequestedAttributes.getLength();
        uno::Sequence< beans::Property > aProperties;
        if (nLenReqAttr)
        {
            const rtl::OUString *pRequestedAttributes = rRequestedAttributes.getConstArray();

            aProperties.realloc( nLenReqAttr );
            beans::Property *pProperties = aProperties.getArray();
            sal_Int32 nCurLen = 0;
            for (sal_Int32 i = 0;  i < nLenReqAttr;  ++i)
            {
                beans::Property aProp;
                try
                {
                    aProp = xPropSetInfo->getPropertyByName( pRequestedAttributes[i] );
                }
                catch (beans::UnknownPropertyException &)
                {
                    continue;
                }
                pProperties[ nCurLen++ ] = aProp;
            }
            aProperties.realloc( nCurLen );
        }
        else
            aProperties = xPropSetInfo->getProperties();

        sal_Int32 nLength = aProperties.getLength();
        const beans::Property *pProperties = aProperties.getConstArray();

        // build resulting sequence
        uno::Sequence< beans::PropertyValue > aOutSequence( nLength );
        beans::PropertyValue* pOutSequence = aOutSequence.getArray();
        sal_Int32 nOutLen = 0;
        for (sal_Int32 i = 0;  i < nLength;  ++i)
        {
            // calling 'regular' functions that will operate on the selection
            PropertyState eState = aPropSet.getPropertyState( pProperties->Name );
            if (eState == PropertyState_DIRECT_VALUE)
            {
                pOutSequence->Name      = pProperties->Name;
                pOutSequence->Handle    = pProperties->Handle;
                pOutSequence->Value     = aPropSet.getPropertyValue( pProperties->Name );
                pOutSequence->State     = eState;

                ++pOutSequence;
                ++nOutLen;
            }
            ++pProperties;
        }
        aOutSequence.realloc( nOutLen );

        return aOutSequence;
    }
    
    // XAccessibleHypertext
    ::sal_Int32 SAL_CALL AccessibleEditableTextPara::getHyperLinkCount(  ) throw (::com::sun::star::uno::RuntimeException)
    {
        SvxAccessibleTextAdapter& rT = GetTextForwarder();
        const sal_Int32 nPara = GetParagraphIndex();

        sal_uInt16 nHyperLinks = 0;
        sal_uInt16 nFields = rT.GetFieldCount( nPara );
        for ( sal_uInt16 n = 0; n < nFields; n++ )
        {
            EFieldInfo aField = rT.GetFieldInfo( nPara, n );
            if ( aField.pFieldItem->GetField()->ISA( SvxURLField ) )
                nHyperLinks++;
        }
        return nHyperLinks;
    }

    ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessibleHyperlink > SAL_CALL AccessibleEditableTextPara::getHyperLink( ::sal_Int32 nLinkIndex ) throw (::com::sun::star::lang::IndexOutOfBoundsException, ::com::sun::star::uno::RuntimeException)
    {
        ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessibleHyperlink > xRef;

        SvxAccessibleTextAdapter& rT = GetTextForwarder();
        const sal_Int32 nPara = GetParagraphIndex();

        sal_uInt16 nHyperLink = 0;
        sal_uInt16 nFields = rT.GetFieldCount( nPara );
        for ( sal_uInt16 n = 0; n < nFields; n++ )
        {
            EFieldInfo aField = rT.GetFieldInfo( nPara, n );
            if ( aField.pFieldItem->GetField()->ISA( SvxURLField ) )
            {
                if ( nHyperLink == nLinkIndex )
                {
                    sal_uInt16 nEEStart = aField.aPosition.nIndex;

                    // Translate EE Index to accessible index
                    sal_uInt16 nStart = rT.CalcEditEngineIndex( nPara, nEEStart );
                    sal_uInt16 nEnd = nStart + aField.aCurrentText.Len();
                    xRef = new AccessibleHyperlink( rT, new SvxFieldItem( *aField.pFieldItem ), nPara, nEEStart, nStart, nEnd, aField.aCurrentText );
                    break;
                }
                nHyperLink++;
            }
        }

        return xRef;
    }
    
    ::sal_Int32 SAL_CALL AccessibleEditableTextPara::getHyperLinkIndex( ::sal_Int32 nCharIndex ) throw (::com::sun::star::lang::IndexOutOfBoundsException, ::com::sun::star::uno::RuntimeException)
    {
        const sal_Int32 nPara = GetParagraphIndex();
        SvxAccessibleTextAdapter& rT = GetTextForwarder();
        
//        SvxAccessibleTextIndex aIndex;
//        aIndex.SetIndex(nPara, nCharIndex, rT);
//        const sal_uInt16 nEEIndex = aIndex.GetEEIndex();

        const sal_uInt16 nEEIndex = rT.CalcEditEngineIndex( nPara, nCharIndex );
        sal_Int32 nHLIndex = 0;
        sal_uInt16 nHyperLink = 0;
        sal_uInt16 nFields = rT.GetFieldCount( nPara );
        for ( sal_uInt16 n = 0; n < nFields; n++ )
        {
            EFieldInfo aField = rT.GetFieldInfo( nPara, n );
            if ( aField.pFieldItem->GetField()->ISA( SvxURLField ) )
            {
                if ( aField.aPosition.nIndex == nEEIndex )
                {
                    nHLIndex = nHyperLink;
                    break;
                }
                nHyperLink++;
            }
        }
        
        return nHLIndex;
    }
    
    // XAccessibleMultiLineText
    sal_Int32 SAL_CALL AccessibleEditableTextPara::getLineNumberAtIndex( sal_Int32 nIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        sal_Int32 nRes = -1;
        sal_Int32 nPara = GetParagraphIndex();

        SvxTextForwarder &rCacheTF = GetTextForwarder();
        const bool bValidPara = 0 <= nPara && nPara < rCacheTF.GetParagraphCount();
        DBG_ASSERT( bValidPara, "getLineNumberAtIndex: current paragraph index out of range" );
        if (bValidPara)
        {
            // we explicitly allow for the index to point at the character right behind the text
            if (0 <= nIndex && nIndex <= rCacheTF.GetTextLen( static_cast< sal_uInt16 >(nPara) ))
                nRes = rCacheTF.GetLineNumberAtIndex( static_cast< sal_uInt16 >(nPara), static_cast< sal_uInt16 >(nIndex) );
            else
                throw lang::IndexOutOfBoundsException();
        }
        return nRes;
    }

    // XAccessibleMultiLineText
    ::com::sun::star::accessibility::TextSegment SAL_CALL AccessibleEditableTextPara::getTextAtLineNumber( sal_Int32 nLineNo ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::com::sun::star::accessibility::TextSegment aResult;
        sal_Int32 nPara = GetParagraphIndex();
        SvxTextForwarder &rCacheTF = GetTextForwarder();
        const bool bValidPara = 0 <= nPara && nPara < rCacheTF.GetParagraphCount();
        DBG_ASSERT( bValidPara, "getTextAtLineNumber: current paragraph index out of range" );
        if (bValidPara)
        {
            if (0 <= nLineNo && nLineNo < rCacheTF.GetLineCount( static_cast< sal_uInt16 >(nPara) ))
            {
                sal_uInt16 nStart = 0, nEnd = 0;
                rCacheTF.GetLineBoundaries( nStart, nEnd, static_cast< sal_uInt16 >(nPara), static_cast< sal_uInt16 >(nLineNo) );
                if (nStart != 0xFFFF && nEnd != 0xFFFF)
                {
                    try
                    {
                        aResult.SegmentText     = getTextRange( nStart, nEnd );
                        aResult.SegmentStart    = nStart;
                        aResult.SegmentEnd      = nEnd;
                    }
                    catch (lang::IndexOutOfBoundsException)
                    {
                        // this is not the exception that should be raised in this function ...
                        DBG_ASSERT( 0, "unexpected exception" );
                    }
                }
            }
            else
                throw lang::IndexOutOfBoundsException();
        }
        return aResult;
    }

    // XAccessibleMultiLineText
    ::com::sun::star::accessibility::TextSegment SAL_CALL AccessibleEditableTextPara::getTextAtLineWithCaret(  ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        ::com::sun::star::accessibility::TextSegment aResult;
        try
        {
            aResult = getTextAtLineNumber( getNumberOfLineWithCaret() );
        }
        catch (lang::IndexOutOfBoundsException &)
        {
            // this one needs to be catched since this interface does not allow for it.
        }
        return aResult;
    }

    // XAccessibleMultiLineText
    sal_Int32 SAL_CALL AccessibleEditableTextPara::getNumberOfLineWithCaret(  ) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        sal_Int32 nRes = -1;
        try
        {
            nRes = getLineNumberAtIndex( getCaretPosition() );
        }
        catch (lang::IndexOutOfBoundsException &)
        {
            // this one needs to be catched since this interface does not allow for it.
        }
        return nRes;
    }


	// XServiceInfo
    ::rtl::OUString SAL_CALL AccessibleEditableTextPara::getImplementationName (void) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        return ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM ("AccessibleEditableTextPara"));
    }

    sal_Bool SAL_CALL AccessibleEditableTextPara::supportsService (const ::rtl::OUString& sServiceName) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        //  Iterate over all supported service names and return true if on of them
        //  matches the given name.
        uno::Sequence< ::rtl::OUString> aSupportedServices (
            getSupportedServiceNames ());
        for (int i=0; i<aSupportedServices.getLength(); i++)
            if (sServiceName == aSupportedServices[i])
                return sal_True;
        return sal_False;
    }

    uno::Sequence< ::rtl::OUString> SAL_CALL AccessibleEditableTextPara::getSupportedServiceNames (void) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        const ::rtl::OUString sServiceName( getServiceName() );
        return uno::Sequence< ::rtl::OUString > (&sServiceName, 1);
    }

	// XServiceName
    ::rtl::OUString SAL_CALL AccessibleEditableTextPara::getServiceName (void) throw (uno::RuntimeException)
    {
        DBG_CHKTHIS( AccessibleEditableTextPara, NULL );

        // #105185# Using correct service now
        return ::rtl::OUString(RTL_CONSTASCII_USTRINGPARAM("com.sun.star.text.AccessibleParagraphView"));
    }

}  // end of namespace accessibility

//------------------------------------------------------------------------
