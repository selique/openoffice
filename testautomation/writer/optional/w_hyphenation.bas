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
'* short description : Tools / Hyphenation test
'*
'\*******************************************************************

sub main

    use "writer\tools\includes\w_tools.inc"
    use "writer\tools\includes\w_tool4.inc"
    use "writer\tools\includes\w_tools_hyphenation.inc"
    use "writer\optional\includes\hyphenation\w_hyphenation.inc"

    printlog Chr(13) + "******* Writer - Hyphenation - Test *******"

    Call hStatusIn ( "writer" , "w_hyphenation.bas" )
    Call wEnableHyphenation(true)
    Call tHyphenation_1
    Call tHyphenation_2    
    Call tHyphenation_3
    Call tHyphenation_4
    Call wEnableHyphenation(false)

    Call hStatusOut

end sub

sub LoadIncludeFiles
    use "global\system\includes\master.inc"
    use "global\system\includes\gvariabl.inc"
    Call GetUseFiles
    gApplication = "WRITER"
end sub
