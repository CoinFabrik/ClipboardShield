Option Explicit
Const ForReading = 1
Dim nVersionMajor, nVersionMinor, nVersionRevision
Dim cCurrentDate, nCurrentBuildCounter
Dim S, S2, szFolder, szConfigFile
Dim fso, xmlDoc, xslDoc, xmlElemRoot, xmlElem, xmlElem2
Dim bForceRebuild

bForceRebuild = False

nVersionMajor    = 1
nVersionMinor    = 0
nVersionRevision = 6

szFolder = Left(WScript.ScriptFullName, Len(WScript.ScriptFullName) - Len(WScript.ScriptName))
Set fso = CreateObject("Scripting.FileSystemObject")

'Read configuration
szConfigFile = szFolder & "autoincrement.config.xml"
If fso.FileExists(szConfigFile) Then
    Err.Clear
    On Error Resume Next
    Set xmlDoc = CreateObject("Microsoft.XMLDOM")
    xmlDoc.Async = False
    xmlDoc.Load(szConfigFile)
    If Err.Number <> 0 Then
        WScript.Echo "Error: Cannot load configuration settings."
        WScript.Quit 1
    End If

    Set xmlElem = xmlDoc.selectSingleNode("/Config/Version/Major")
    If (xmlElem Is Nothing) Or (nVersionMajor <> CInt(Trim(xmlElem.Text))) Then bForceRebuild = True
    Set xmlElem = xmlDoc.selectSingleNode("/Config/Version/Minor")
    If (xmlElem Is Nothing) Or (nVersionMinor <> CInt(Trim(xmlElem.Text))) Then bForceRebuild = True
    Set xmlElem = xmlDoc.selectSingleNode("/Config/Version/Revision")
    If (xmlElem Is Nothing) Or (nVersionRevision <> CInt(Trim(xmlElem.Text))) Then bForceRebuild = True

    Set xmlElem = xmlDoc.selectSingleNode("/Config/LastBuildNumber")
    If xmlElem Is Nothing Then
        WScript.Echo "Error: Missing LastBuildNumber setting."
        WScript.Quit 1
    End If
    Err.Clear
    nCurrentBuildCounter = CInt(Trim(xmlElem.Text))
    If Err.Number <> 0 Then
        WScript.Echo "Error: LastBuildNumber setting contains invalid data."
        WScript.Quit 1
    End If

    Set xmlElem = xmlDoc.selectSingleNode("/Config/LastBuildDate")
    If xmlElem Is Nothing Then
        WScript.Echo "Error: Missing LastBuildDate setting."
        WScript.Quit 1
    End If
    Err.Clear
    cCurrentDate = CDate(Trim(xmlElem.Text))
    If Err.Number <> 0 Then
        WScript.Echo "Error: LastBuildDate setting contains invalid data."
        WScript.Quit 1
    End If

    On Error Goto 0
    Set xmlElem = Nothing
    Set xmlDoc = Nothing
Else
    nCurrentBuildCounter = 0
    cCurrentDate = Date
End If

'If date is the same, version.* files exists and build counter is greater than zero, abort
If bForceRebuild = False And _
        nCurrentBuildCounter > 0 And _
        DateDiff("d", cCurrentDate, Date) = 0 And _
        fso.FileExists(szFolder & "version.h") <> False Then
    WScript.Quit 0
End If

'Increment build counter
nCurrentBuildCounter = nCurrentBuildCounter + 1


'Write the new C++ include file
S = "#define __VERSION_VALUE  " & NumberToString(nVersionMajor, 0) & ", " & NumberToString(nVersionMinor, 0) & ", " & NumberToString(nVersionRevision, 0) & ", " & NumberToString(nCurrentBuildCounter, 0) & vbCrLf
S = S & "#define __VERSION_TEXT  " & Chr(34) & NumberToString(nVersionMajor, 0) & "." & NumberToString(nVersionMinor, 0) & "." & NumberToString(nVersionRevision, 0) & "." & NumberToString(nCurrentBuildCounter, 0) & Chr(34) & vbCrLf
S = S & "#define __VERSION_TEXT_W  L" & Chr(34) & NumberToString(nVersionMajor, 0) & "." & NumberToString(nVersionMinor, 0) & "." & NumberToString(nVersionRevision, 0) & "." & NumberToString(nCurrentBuildCounter, 0) & Chr(34) & vbCrLf
S2 = NumberToString(DatePart("yyyy", Date), 4)
S = S & "#define __BUILD_YEAR_VALUE  " & S2 & vbCrLf
S = S & "#define __BUILD_YEAR_TEXT  " & Chr(34) & S2 & Chr(34) & vbCrLf
S = S & "#define __BUILD_YEAR_TEXT_W  L" & Chr(34) & S2 & Chr(34) & vbCrLf
If WriteTextFile(fso, szFolder & "version.h", S) = False Then
    WScript.Echo "Error: Cannot write to 'version.h' file"
    WScript.Quit 1
End If


'Store configuration settings
Set xmlDoc = CreateObject("Microsoft.XMLDOM")

Set xmlElemRoot = xmlDoc.createElement("Config")
xmlDoc.appendChild xmlElemRoot

Set xmlElem = xmlDoc.createElement("Version")
xmlElemRoot.appendChild xmlElem

Set xmlElem2 = xmlDoc.createElement("Major")
xmlElem.appendChild xmlElem2
xmlElem2.Text = Trim(CStr(nVersionMajor))

Set xmlElem2 = xmlDoc.createElement("Minor")
xmlElem.appendChild xmlElem2
xmlElem2.Text = Trim(CStr(nVersionMinor))

Set xmlElem2 = xmlDoc.createElement("Revision")
xmlElem.appendChild xmlElem2
xmlElem2.Text = Trim(CStr(nVersionRevision))

Set xmlElem = xmlDoc.createElement("LastBuildNumber")
xmlElemRoot.appendChild xmlElem
xmlElem.Text = Trim(CStr(nCurrentBuildCounter))

Set xmlElem = xmlDoc.createElement("LastBuildDate")
xmlElemRoot.appendChild xmlElem
xmlElem.Text = Right("0000" & DatePart("yyyy", Date), 4) & "-" & Right("00" & DatePart("m", Date), 2) & "-" & Right("00" & DatePart("d", Date), 2)

'Set xmlElem = xmlDoc.createProcessingInstruction("xml", "version=" & Chr(34) & "1.0" & Chr(34) & " encoding=" & Chr(34) & "UTF-8" & Chr(34) & " standalone=" & Chr(34) & "yes" & Chr(34))
'xmlDoc.insertBefore xmlElem, xmlDoc.childNodes(0)

'Apply stylesheet
S =     "<?xml version=" & Chr(34) & "1.0" & Chr(34) & " encoding=" & Chr(34) & "UTF-8" & Chr(34) & "?>" & vbCrLf
S = S & "<xsl:stylesheet version=" & Chr(34) & "1.0" & Chr(34) & " xmlns:xsl=" & Chr(34) & "http://www.w3.org/1999/XSL/Transform" & Chr(34) & ">" & vbCrLf
S = S & "  <xsl:output method=" & Chr(34) & "xml" & Chr(34) & " version=" & Chr(34) & "1.0" & Chr(34) & " encoding=" & Chr(34) & "UTF-8" & Chr(34) & " standalone=" & Chr(34) & "yes" & Chr(34) & " indent=" & Chr(34) & "yes" & Chr(34) & "/>" & vbCrLf
S = S & "  <xsl:template match=" & Chr(34) & "/ | @* | node()" & Chr(34) & ">" & vbCrLf
S = S & "    <xsl:copy>" & vbCrLf
S = S & "      <xsl:apply-templates select=" & Chr(34) & "@* | node()" & Chr(34) & " />" & vbCrLf
S = S & "    </xsl:copy>" & vbCrLf
S = S & "  </xsl:template>" & vbCrLf
S = S & "</xsl:stylesheet>"

Set xslDoc = CreateObject("Microsoft.XMLDOM")
xslDoc.async = false
xslDoc.loadXML(S)
xmlDoc.transformNodeToObject xslDoc, xmlDoc

'Save
On Error Resume Next
Err.Clear
xmlDoc.Save szConfigFile
If Err.Number <> 0 Then
    WScript.Echo "Error: Cannot save configuration settings."
    WScript.Quit 1
End If
On Error Goto 0

WScript.Quit 0

'-------------------------------------------------------------------------------

Function NumberToString(V, nMinDigits)
Dim S

    S = Trim(CStr(V))
    If Len(S) < nMinDigits Then
        S = Right("0000000000000000000" & S, nMinDigits)
    End If
    NumberToString = S
End Function

Function WriteTextFile(fso, szFileName, szText)
Dim textFile

    WriteTextFile = False
    Err.Clear
    On Error Resume Next
    Set textFile = fso.CreateTextFile(szFileName)
    If Err.Number <> 0 Then Exit Function
    textFile.Write szText
    If Err.Number <> 0 Then Exit Function
    textFile.Close
    On Error Goto 0
    WriteTextFile = True
End Function
