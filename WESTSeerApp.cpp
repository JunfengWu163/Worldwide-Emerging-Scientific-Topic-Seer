/***************************************************************
 * Name:      WESTSeerApp.cpp
 * Purpose:   Code for Application Class
 * Author:    Junfeng Wu (wujunfeng@vip.163.com)
 * Created:   2023-06-11
 * Copyright: Junfeng Wu (https://westseer.org)
 * License:
 **************************************************************/

#include "WESTSeerApp.h"
#include <wx/stdpaths.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <GeneralConfig.h>
#include <AbstractTask.h>
#include "SettingsDialog.h"
#include <string>
#include <ctime>

//(*AppHeaders
#include "WESTSeerMain.h"
#include <wx/image.h>
//*)

IMPLEMENT_APP(WESTSeerApp);

wxFileConfig *WESTSeerApp::_fileConfig = NULL;
wxFFileLog *WESTSeerApp::_log = NULL;
int WESTSeerApp::_year;

wxFileConfig *WESTSeerApp::getFileConfig()
{
    return _fileConfig;
}

void WESTSeerApp::FlushLog()
{
    _log->Flush();
}

int WESTSeerApp::year()
{
    return _year;
}

wxString WESTSeerApp::modelPath()
{
    GeneralConfig config;
    int k = config.getBiterms();
    wxFileName appFile(wxStandardPaths::Get().GetExecutablePath());
    wxString appPath(appFile.GetPath());
    wxString relModelPath = "\\models";
    wxFileName modelFile(appPath + relModelPath, wxString::Format("lstm_%d.pb", k));
    for (int i = 0; i < 10; i++)
    {
        if (modelFile.Exists())
            return appPath + relModelPath;
        relModelPath = "\\.." + relModelPath;
        modelFile = wxFileName(appPath + relModelPath, wxString::Format("lstm_%d.pb", k));
    }
    logError("Model path cannot be found.");
    return appPath;
}

wxString WESTSeerApp::modelFileName()
{
    GeneralConfig config;
    int k = config.getBiterms();
    wxString path = modelPath();
    wxFileName fileName(path, wxString::Format("lstm_%d.pb", k));
    return fileName.GetAbsolutePath();
}

bool WESTSeerApp::OnInit()
{
    // get current year
    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);
    _year = now->tm_year + 1900;

    // create app directory if not exists
    wxString appDir = wxStandardPaths::Get().GetUserLocalDataDir();
	if (!wxDirExists(appDir))
    {
        wxMkdir(appDir);
    }

    // create INI file in user's directory
	wxString iniFileName = wxFileName(appDir, "config.ini").GetFullPath();
	_fileConfig = new wxFileConfig(wxT("WESTSeer"), wxT("westseer.org"), iniFileName);
	wxConfigBase::Set(_fileConfig);

	// start logging
	wxString logFileName(wxFileName(appDir, "log.txt").GetFullPath());
	if (wxFileExists(logFileName))
    {
        wxString oldLogFileName1(wxFileName(appDir, "log.old.1.txt").GetFullPath());
        if (wxFileExists(oldLogFileName1))
        {
            wxString oldLogFileName2(wxFileName(appDir, "log.old.2.txt").GetFullPath());
            if (wxFileExists(oldLogFileName2))
            {
                wxString oldLogFileName3(wxFileName(appDir, "log.old.3.txt").GetFullPath());
                wxRenameFile(oldLogFileName2, oldLogFileName3);
            }
            wxRenameFile(oldLogFileName1, oldLogFileName2);
        }
        wxRenameFile(logFileName, oldLogFileName1);
    }
    _log = new wxFFileLog(logFileName);
    if (_log->IsLogFileOK())
    {
        _logChain = new wxLogChain(_log);
    }
    else
    {
        wxString msg;

        msg.Printf(_("Could not initialize the application log (file \"%s\"). \n\nContinue anyway?"),
            _log->GetLogFileName());
        delete _log;

        if ( wxMessageBox(msg, _("Error"), wxYES_NO | wxICON_ERROR, NULL)
             != wxYES )
        {
            return false;
        }
    }

    // first time init
    std::string email;
    {
        GeneralConfig config;
        email = config.getEmail();
    }
    if (email == "")
    {
        SettingsDialog dlg(NULL);
        dlg.ShowModal();
        GeneralConfig config;
        email = config.getEmail();
        if (email == "")
            return false;
    }

    //(*AppInitialize
    bool wxsOK = true;
    wxInitAllImageHandlers();
    if ( wxsOK )
    {
    	WESTSeerFrame* Frame = new WESTSeerFrame(0);
    	Frame->Show();
    	SetTopWindow(Frame);
    }
    //*)
    return wxsOK;

}

int WESTSeerApp::OnExit()
{
    AbstractTask::finalize();
    if(_fileConfig) {
		_fileConfig->Flush();
		delete _fileConfig;
		_fileConfig=NULL;
	}
	if (_logChain) {
        wxLog::SetActiveTarget(NULL);
        delete _logChain;
        _log = NULL;
	}
	wxConfigBase::Set(NULL);
	return 0;
}
