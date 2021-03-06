//////////////////////////////////////////////////////////////////////////
//
//  UltraDefrag - a powerful defragmentation tool for Windows NT.
//  Copyright (c) 2007-2017 Dmitri Arkhangelski (dmitriar@gmail.com).
//  Copyright (c) 2010-2013 Stefan Pendl (stefanpe@users.sourceforge.net).
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//////////////////////////////////////////////////////////////////////////

/**
 * @file menu.cpp
 * @brief Menu.
 * @note On Windows 7 menu icons and check marks ain't centered,
 * but it looks like it's by design as they ain't centered in
 * Windows Explorer as well.
 * @addtogroup Menu
 * @{
 */

// Ideas by Stefan Pendl <stefanpe@users.sourceforge.net>
// and Dmitri Arkhangelski <dmitriar@gmail.com>.

// =======================================================================
//                            Declarations
// =======================================================================

#include "prec.h"
#include "main.h"

/*
* wxWidgets raises a lot of "assert failure"
* messages if wxEmptyString is used instead
*/
#define EmptyLabel wxT(" ")

#define UD_AppendItem(menu,id,icon) { \
    wxMenuItem *item = new wxMenuItem(NULL,id,EmptyLabel); \
    wxString s(icon); \
    if(!s.IsEmpty() && CheckOption(wxT("UD_SHOW_MENU_ICONS"))){ \
        s << wxString::Format("%u",g_iconSize); \
        wxBitmap pic = Utils::LoadPngResource(ws(s)); \
        if(pic.IsOk()) item->SetBitmap(pic); \
    } \
    menu->Append(item); \
}

#define UD_AppendCheckItem(menu,id) menu->AppendCheckItem(id,EmptyLabel)
#define UD_AppendRadioItem(menu,id) menu->AppendRadioItem(id,EmptyLabel)
#define UD_AppendSeparator(menu)    menu->AppendSeparator();

#define UD_SetMarginWidth(menu) { \
    wxMenuItemList list = menu->GetMenuItems(); \
    size_t count = list.GetCount(); \
    for(size_t i = 0; i < count; i++) \
        list.Item(i)->GetData()->SetMarginWidth(g_iconSize + DPI(4)); \
}

// =======================================================================
//                        Menu for main window
// =======================================================================

/**
 * @brief Initializes main menu. Uses i18n.cpp for localization strings
 */
void MainFrame::InitMenu()
{
    // create when done menu
    wxMenu *menuWhenDone = new wxMenu;
    UD_AppendRadioItem(menuWhenDone, ID_WhenDoneNone);
    UD_AppendRadioItem(menuWhenDone, ID_WhenDoneExit);
    UD_AppendRadioItem(menuWhenDone, ID_WhenDoneStandby);
    UD_AppendRadioItem(menuWhenDone, ID_WhenDoneHibernate);
    UD_AppendRadioItem(menuWhenDone, ID_WhenDoneLogoff);
    UD_AppendRadioItem(menuWhenDone, ID_WhenDoneReboot);
    UD_AppendRadioItem(menuWhenDone, ID_WhenDoneShutdown);

    // create action menu
    wxMenu *m_menuAction = new wxMenu;
    UD_AppendItem(m_menuAction,      ID_Analyze,    wxT("glass") );
    UD_AppendItem(m_menuAction,      ID_Defrag,     wxT("defrag"));
    UD_AppendItem(m_menuAction,      ID_QuickOpt,   wxT("quick") );
    UD_AppendItem(m_menuAction,      ID_FullOpt,    wxT("full")  );
    UD_AppendItem(m_menuAction,      ID_MftOpt,     wxT("mft")   );
    UD_AppendCheckItem(m_menuAction, ID_Pause                    );
    UD_AppendItem(m_menuAction,      ID_Stop,       wxT("stop")  );
    UD_AppendSeparator(m_menuAction);

    UD_AppendItem(m_menuAction,      ID_ShowReport, wxT("report"));
    UD_AppendSeparator(m_menuAction);

    UD_AppendCheckItem(m_menuAction, ID_SkipRem                  );
    UD_AppendItem(m_menuAction,      ID_Rescan,     wxEmptyString);
    UD_AppendSeparator(m_menuAction);

    UD_AppendItem(m_menuAction,      ID_Repair,     wxEmptyString);
    UD_AppendSeparator(m_menuAction);

    m_subMenuWhenDone = m_menuAction->AppendSubMenu(menuWhenDone, EmptyLabel);
    UD_AppendSeparator(m_menuAction);

    UD_AppendItem(m_menuAction,      ID_Exit,       wxEmptyString);

    // create Query menu (by genBTC) - runs code in Query.cpp
    //title is set @ i18n.cpp Line 192, and added to the menubar @ 252 below.
    wxMenu *menuQuery = new wxMenu;
    UD_AppendItem(menuQuery, ID_QueryClusters, wxEmptyString);    //query the internals and ask what clusters a file uses.
    UD_AppendSeparator(menuQuery);
    UD_AppendItem(menuQuery, ID_QueryFreeGaps, wxEmptyString);    //query the internals ask where the free gap regions are
    UD_AppendItem(menuQuery, ID_QueryOperation2, wxEmptyString);    // Operation 2
    UD_AppendItem(menuQuery, ID_QueryOperation3, wxEmptyString);    // Operation 3
    UD_AppendItem(menuQuery, ID_QueryOperation4, wxEmptyString);    // Operation 4
    //append more items to query tab here:
    
    // create language menu
    m_menuLanguage = new wxMenu;
    UD_AppendItem(m_menuLanguage, ID_LangTranslateOnline,  wxEmptyString);
    UD_AppendItem(m_menuLanguage, ID_LangTranslateOffline, wxEmptyString);
    UD_AppendItem(m_menuLanguage, ID_LangOpenFolder,       wxEmptyString);
    UD_AppendSeparator(m_menuLanguage);

    wxString AppLocaleDir(wxGetCwd());
    AppLocaleDir.Append(wxT("/locale"));
    if(!wxDirExists(AppLocaleDir)){
        itrace("lang dir not found: %ls",ws(AppLocaleDir));
        AppLocaleDir = wxGetCwd() + wxT("/../wxgui/locale");
    }
    if(!wxDirExists(AppLocaleDir)){
        etrace("lang dir not found: %ls",ws(AppLocaleDir));
        AppLocaleDir = wxGetCwd() + wxT("/../../wxgui/locale");
    }

    wxDir dir(AppLocaleDir);
    const wxLanguageInfo *info;

    if(!dir.IsOpened()){
        etrace("can't open lang dir: %ls",ws(AppLocaleDir));
        info = g_locale->FindLanguageInfo(wxT("en_US"));
        m_menuLanguage->AppendRadioItem(ID_LocaleChange \
            + info->Language, info->Description);
    } else {
        wxString folder;
        wxArrayString langArray;

        bool cont = dir.GetFirst(&folder, wxT("*"), wxDIR_DIRS);

        while(cont){
            info = g_locale->FindLanguageInfo(folder);
            if(info){
                if(info->Description == wxT("Chinese")){
                    langArray.Add(wxT("Chinese (Traditional)"));
                } else {
                    if(info->Description == wxT("English")){
                        langArray.Add(wxT("English (U.K.)"));
                    } else {
                        langArray.Add(info->Description);
                    }
                }
            } else {
                etrace("can't find locale info for %ls",ws(folder));
            }
            cont = dir.GetNext(&folder);
        }

        langArray.Sort();

        // divide list of languages to three columns
        unsigned int breakDelta = (unsigned int)ceil((double) \
            (langArray.Count() + langArray.Count() % 2 + 4) / 3);
        unsigned int breakCnt = breakDelta - 4;
        itrace("languages: %d, break count: %d, delta: %d",
            langArray.Count(), breakCnt, breakDelta);
        for(unsigned int i = 0;i < langArray.Count();i++){
            info = g_locale->FindLanguageInfo(langArray[i]);
            m_menuLanguage->AppendRadioItem(ID_LocaleChange \
                + info->Language, info->Description);
            if((i+1) % breakCnt == 0){
                m_menuLanguage->Break();
                breakCnt += breakDelta;
            }
        }
    }

    // create settings menu
    wxMenu *menuSettings = new wxMenu;
    m_subMenuLanguage = menuSettings-> \
        AppendSubMenu(m_menuLanguage, EmptyLabel);
    UD_AppendItem(menuSettings, ID_GuiOptions, wxT("gear"));

    // create sorting configuration menu
    wxMenu *menuSortingConfig = new wxMenu;
    UD_AppendRadioItem(menuSortingConfig, ID_SortByPath);
    UD_AppendRadioItem(menuSortingConfig, ID_SortBySize);
    UD_AppendRadioItem(menuSortingConfig, ID_SortByCreationDate);
    UD_AppendRadioItem(menuSortingConfig, ID_SortByModificationDate);
    UD_AppendRadioItem(menuSortingConfig, ID_SortByLastAccessDate);
    UD_AppendSeparator(menuSortingConfig);
    UD_AppendRadioItem(menuSortingConfig, ID_SortAscending);
    UD_AppendRadioItem(menuSortingConfig, ID_SortDescending);
    m_subMenuSortingConfig = menuSettings-> \
        AppendSubMenu(menuSortingConfig, EmptyLabel);    

    // create boot configuration menu
    wxMenu *menuBootConfig = new wxMenu;
    UD_AppendCheckItem(menuBootConfig, ID_BootEnable);
    UD_AppendItem(menuBootConfig, ID_BootScript, wxT("script"));
    m_subMenuBootConfig = menuSettings-> \
        AppendSubMenu(menuBootConfig, EmptyLabel);
    //create font dropdown menu
    UD_AppendItem(menuSettings, ID_ChooseFont, wxEmptyString); //genBTC font dialog.
    // create help menu
    wxMenu *menuHelp = new wxMenu;
    UD_AppendItem(menuHelp, ID_HelpContents, wxT("help"));
    UD_AppendSeparator(menuHelp);

    UD_AppendItem(menuHelp, ID_HelpBestPractice, wxT("light"));
    UD_AppendItem(menuHelp, ID_HelpFaq, wxEmptyString);
    UD_AppendItem(menuHelp, ID_HelpLegend, wxEmptyString);
    UD_AppendSeparator(menuHelp);

    // create debug menu
    wxMenu *menuDebug = new wxMenu;
    UD_AppendItem(menuDebug, ID_DebugLog,  wxEmptyString);
    UD_AppendItem(menuDebug, ID_DebugSend, wxEmptyString);
    m_subMenuDebug = menuHelp->AppendSubMenu(menuDebug, EmptyLabel);
    UD_AppendSeparator(menuHelp);

    // create upgrade menu
    wxMenu *menuUpgrade = new wxMenu;
    UD_AppendRadioItem(menuUpgrade, ID_HelpUpgradeNone);
    UD_AppendRadioItem(menuUpgrade, ID_HelpUpgradeStable);
    UD_AppendRadioItem(menuUpgrade, ID_HelpUpgradeAll);
    UD_AppendSeparator(menuUpgrade);
    UD_AppendItem(menuUpgrade, ID_HelpUpgradeCheck, wxEmptyString);
    m_subMenuUpgrade = menuHelp->AppendSubMenu(menuUpgrade, EmptyLabel);
    UD_AppendSeparator(menuHelp);
    UD_AppendItem(menuHelp, ID_HelpAbout, wxT("star"));

    // create main menu
    m_menuBar = new wxMenuBar;
    m_menuBar->Append(m_menuAction, EmptyLabel);
    m_menuBar->Append(menuQuery   , EmptyLabel);    //genBTC query menu
    m_menuBar->Append(menuSettings, EmptyLabel);
    m_menuBar->Append(menuHelp    , EmptyLabel);
    SetMenuBar(m_menuBar);

    // set margin width
    if(CheckOption(wxT("UD_SHOW_MENU_ICONS"))){
        UD_SetMarginWidth(m_menuBar->GetMenu(0));
        UD_SetMarginWidth(m_menuBar->GetMenu(1));
        UD_SetMarginWidth(m_menuBar->GetMenu(2));
        UD_SetMarginWidth(m_menuBar->GetMenu(3));
        UD_SetMarginWidth(menuBootConfig);
    }

    // initial settings
    m_menuBar->Check(ID_SkipRem,m_skipRem);

    int id = g_locale->GetLanguage();
    wxMenuItem *item = m_menuBar->FindItem(ID_LocaleChange + id);
    if(item) item->Check(true);

    wxConfigBase *cfg = wxConfigBase::Get();
    wxString sorting = cfg->Read(wxT("/Algorithm/Sorting"),wxT("path"));
    if(sorting == wxT("path")){
        m_menuBar->Check(ID_SortByPath,true);
    } else if(sorting == wxT("size")){
        m_menuBar->Check(ID_SortBySize,true);
    } else if(sorting == wxT("c_time")){
        m_menuBar->Check(ID_SortByCreationDate,true);
    } else if(sorting == wxT("m_time")){
        m_menuBar->Check(ID_SortByModificationDate,true);
    } else if(sorting == wxT("a_time")){
        m_menuBar->Check(ID_SortByLastAccessDate,true);
    }
    wxString order = cfg->Read(wxT("/Algorithm/SortingOrder"),wxT("asc"));
    if(order == wxT("asc")){
        m_menuBar->Check(ID_SortAscending,true);
    } else {
        m_menuBar->Check(ID_SortDescending,true);
    }
}

/** @} */
