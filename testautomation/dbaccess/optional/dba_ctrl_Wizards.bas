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
'*
'* short description : Control Wizard Test
'*
'\***********************************************************************
sub main
    printlog "---------------------------------------------------------------------"
    printlog "-------    D B A C C E S S   C O N T R O L   W I Z A R D S    -------"
    printlog "---------------------------------------------------------------------"

    use "dbaccess/optional/includes/ctrl_Wizards.inc"

    call hStatusIn ("dbaccess" , "dba_ctrl_Wizards.bas")
	
	call ctrl_Wizards

	call hStatusOut

end sub


sub LoadIncludeFiles   
   use "dbaccess/tools/dbinit.inc"
   Call sDBInit
   Call GetUseFiles
   gApplication   = "WRITER"
end sub
