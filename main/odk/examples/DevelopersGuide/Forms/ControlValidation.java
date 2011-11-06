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



import com.sun.star.uno.*;
import com.sun.star.util.*;
import com.sun.star.lang.*;
import com.sun.star.accessibility.*;
import com.sun.star.container.*;
import com.sun.star.beans.*;
import com.sun.star.form.binding.*;

/**
 *
 * @author  fs@openoffice.org
 */
public class ControlValidation extends DocumentBasedExample
{
    /** Creates a new instance of ControlValidation */
    public ControlValidation()
    {
        super( DocumentType.WRITER );
    }

    /* ------------------------------------------------------------------ */
    /* public test methods                                                */
    /* ------------------------------------------------------------------ */
    protected void prepareDocument() throws com.sun.star.uno.Exception, java.lang.Exception
    {
        super.prepareDocument();

        SingleControlValidation validation;
        XPropertySet focusField;

        validation = new SingleControlValidation( m_document, 5, 5, "DatabaseFormattedField", new NumericValidator() );
        focusField = validation.getInputField();
        validation.setExplanatoryText( "Please enter a number between 0 and 100, with at most 1 decimal digit" );

        validation = new SingleControlValidation( m_document, 90, 5, "DatabaseTextField", new TextValidator() );
        validation.setExplanatoryText( "Please enter a text whose length is a multiple of 3, and which does not contain the letter 'Z'" );

        validation = new SingleControlValidation( m_document, 5, 55, "DatabaseDateField", new DateValidator() );
        validation.setExplanatoryText( "Please enter a date in the current month" );
        validation.getInputField().setPropertyValue( "Dropdown", new Boolean( true ) );

        validation = new SingleControlValidation( m_document, 90, 55, "DatabaseTimeField", new TimeValidator() );
        validation.setExplanatoryText( "Please enter a time. Valid values are all full hours." );

        validation = new SingleControlValidation( m_document, 5, 110, "DatabaseCheckBox", new BooleanValidator( false ) );
        validation.setExplanatoryText( "Please check (well, or uncheck) the box. Don't leave it in indetermined state." );
        validation.getInputField().setPropertyValue( "TriState", new Boolean( true ) );

        validation = new SingleControlValidation( m_document, 90, 110, "DatabaseRadioButton", new BooleanValidator( true ), 3, 0 );
        validation.setExplanatoryText( "Please check any but the first button" );

        validation = new SingleControlValidation( m_document, 5, 165, "DatabaseListBox", new ListSelectionValidator( ), 1, 24 );
        validation.setExplanatoryText( "Please select not more than two entries." );
        validation.getInputField().setPropertyValue( "MultiSelection", new Boolean( true ) );
        validation.getInputField().setPropertyValue( "StringItemList", new String[] { "first", "second", "third", "forth", "fivth" } );

        // switch to alive mode
        m_document.getCurrentView( ).toggleFormDesignMode( );
        m_document.getCurrentView( ).grabControlFocus( focusField );

        // wait for the user telling us to exit
        waitForUserInput();
    }

    /* ------------------------------------------------------------------ */
    /** class entry point
    */
    public static void main(String argv[]) throws java.lang.Exception
    {
        ControlValidation aSample = new ControlValidation();
        aSample.run( argv );
    }
}
