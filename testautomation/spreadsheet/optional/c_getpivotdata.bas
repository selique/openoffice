'encoding UTF-8  Do not remove or change this line!
'*************************************************************************
'
'  Licensed to the Apache Software Foundation (ASF) under one
'  or more contributor license agreements.  See the NOTICE file
'  distributed with this work for additional information
'  regarding copyright ownership.  The ASF licenses this file
'  to you under the Apache License, Version 2.0 (the
'  "License"); you may not use this file except in compliance
'  with the License.  You may obtain a copy of the License at
'  
'    http://www.apache.org/licenses/LICENSE-2.0
'  
'  Unless required by applicable law or agreed to in writing,
'  software distributed under the License is distributed on an
'  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
'  KIND, either express or implied.  See the License for the
'  specific language governing permissions and limitations
'  under the License.
'
'*************************************************************************
'* short description : Test for function GETPIVOTDATA in calc
'*
'\***********************************************************************


sub main       
    use "spreadsheet/optional/includes/getpivotdata/c_getpivotdata.inc"
    use "spreadsheet/tools/includes/c_select_tools.inc"
    use "spreadsheet/tools/includes/c_cell_tools.inc"
    use "spreadsheet/tools/includes/c_l10n_tools.inc"
    
    Printlog "--------------------------------------------------"
    Printlog "---  Test for function GETPIVOTDATA in calc    ---"
    Printlog "--------------------------------------------------"
    
    Call hStatusIn("spreadsheet", "c_getpivotdata.bas")                
    
    Call tgetpivotdata_function_wizard
    Call tgetpivotdata_xls_filter
        
    Call hStatusOut
end sub
'
'-------------------------------------------------------------------------------
'
sub LoadIncludeFiles
    use "global/system/includes/master.inc"
    use "global/system/includes/gvariabl.inc"
    use "global/tools/includes/optional/t_spreadsheet_tools1.inc"
    gApplication   = "CALC"
    call getusefiles
    
end sub
