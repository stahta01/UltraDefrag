//////////////////////////////////////////////////////////////////////////
//
//  UltraDefrag - a powerful defragmentation tool for Windows NT.
//  Copyright (c) 2007-2013 Dmitri Arkhangelski (dmitriar@gmail.com).
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
 * @file fileslist.cpp
 * @brief List of files.
 * @addtogroup FilesList
 * @{
 */

// Ideas by Stefan Pendl <stefanpe@users.sourceforge.net>
// and Dmitri Arkhangelski <dmitriar@gmail.com>.

// =======================================================================
//                            Declarations
// =======================================================================

#include "main.h"
#include "udefrag-internals_flags.h"
#include "windows.h"
//#include <string>

int f_fixedIcon;
int f_fixedDirtyIcon;
int f_removableIcon;
int f_removableDirtyIcon;

//genBTC fileslist.cpp right click menu
enum
{
    ID_MNU_MENUITEMONE_1003 = 1003,
    ID_MNU_MENUITEMTWO_1004 = 1004,
    ID_MNU_YETANOTHERMENUITEM_1005 = 1005,
    ID_MNU_YETANOTHERMENUITEM_1006 = 1006,

    ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
};

// =======================================================================
//                           ListView of fragmented files.
// =======================================================================

void MainFrame::InitFilesList()
{
    // save default font used for the list
    m_filesListFont = new wxFont(m_filesList->GetFont());

    // set mono-space font for the list unless Burmese translation is selected
    if(g_locale->GetCanonicalName().Left(2) != wxT("my")){
        wxFont font = m_filesList->GetFont();
        if(font.SetFaceName(wxT("Lucida"))){
            font.SetPointSize(DPI(9));
            m_filesList->SetFont(font);
        }
    }


    // adjust widths so all the columns will fit to the window
    //int width = m_filesList->GetClientSize().GetWidth();
    int width = m_vList->GetClientSize().GetWidth();
    //account for the borders
    int border = wxSystemSettings::GetMetric(wxSYS_BORDER_X);
    int lastColumnWidth = width - border*4;
    dtrace("INIT - client width ......... %d", width);
    dtrace("INIT - border width ......... %d", border);

    int format[] = {
        wxLIST_FORMAT_LEFT, wxLIST_FORMAT_LEFT,
        wxLIST_FORMAT_RIGHT, wxLIST_FORMAT_CENTER,
        wxLIST_FORMAT_CENTER, wxLIST_FORMAT_LEFT
    };

    double ratios[LIST_COLUMNS] = {
            510.0/900, 72.0/900, 78.0/900,
            55.0/900, 55.0/900, 130.0/900
    };

    for(int i = 0; i < LIST_COLUMNS - 1; i++) {
        m_fcolsr[i] = ratios[i];    //initialize with fixed values from above
        int w = m_fcolsw[i] = (int)floor(m_fcolsr[i] * width);        //genBTC - set to default ratios.
        m_filesList->InsertColumn(i, wxEmptyString, format[i], w);
        dtrace("FilesList column %d width ....... %d", i, w);
        lastColumnWidth -= w;
    }

    //initialize with values (needed because the loop above goes 1 column less on purpose
    m_fcolsr[LIST_COLUMNS - 1] = ratios[LIST_COLUMNS - 1];
    //adjust the last column to be the precise size needed.
    int w = (int)floor(m_fcolsr[LIST_COLUMNS - 1] * width);
    if(w > 0) w = lastColumnWidth;
    m_fcolsw[LIST_COLUMNS - 1] = w;
    m_filesList->InsertColumn(LIST_COLUMNS - 1,
        wxEmptyString, format[LIST_COLUMNS - 1], w
    );
    dtrace("FilesList column %d width ....... %d", LIST_COLUMNS - 1, w);

    // attach drive icons
    int size = g_iconSize;
    wxImageList *list = new wxImageList(size,size);
    f_fixedIcon          = list->Add(wxIcon(wxT("fixed")           , wxBITMAP_TYPE_ICO_RESOURCE, size, size));
    f_fixedDirtyIcon     = list->Add(wxIcon(wxT("fixed_dirty")     , wxBITMAP_TYPE_ICO_RESOURCE, size, size));
    f_removableIcon      = list->Add(wxIcon(wxT("removable")       , wxBITMAP_TYPE_ICO_RESOURCE, size, size));
    f_removableDirtyIcon = list->Add(wxIcon(wxT("removable_dirty") , wxBITMAP_TYPE_ICO_RESOURCE, size, size));
    m_filesList->SetImageList(list,wxIMAGE_LIST_SMALL);

    // ensure that the list will cover integral number of items
    m_filesListHeight = 0xFFFFFFFF; // prevent expansion of the list
    m_filesList->InsertItem(0,wxT("hi"),0);
    ProcessCommandEvent(EventID_AdjustFilesListHeight);

    Connect(wxEVT_SIZE,wxSizeEventHandler(MainFrame::FilesOnListSize),NULL,this);
//    m_splitter->Connect(wxEVT_COMMAND_SPLITTER_SASH_POS_CHANGED,
//        wxSplitterEventHandler(MainFrame::FilesOnSplitChanged),NULL,this);

    m_filesList->InitMembers();

}
/*Ideas:
	//Move to First Free Region (that fits fully)
	//Move to Last Free Region that fits fully)
	//Move to First Free Region(s) in order w/Fragmentation allowed
	//Move to Last Free Region(s) in order w/Fragmentation allowed
 * GUI: Maybe make a 3rd page with a "proposed actions pending" list, where you can queue/order them, and a Commit button
 * Disk: Force a dismount of the volume (by removing the drive letter or etc.) so you can re-order all the files with no chance of unexpected changes.
 * GUI: Make currently worked on sector be highlighted in green, or maybe even two colors (read/write).
 * GUI: Click files and have them display which blocks its occupying in the clustermap (later add support for multiple-selected-files)
 *
 *
 *
 *
*/
void FilesList::InitMembers(){
	WxPopupMenu1 = new wxMenu(wxT(""));
	WxPopupMenu1->Append(ID_MNU_MENUITEMONE_1003, wxT("Defragment Now"), wxT(""), wxITEM_NORMAL);
	WxPopupMenu1->Append(ID_MNU_MENUITEMTWO_1004, wxT("Open in Explorer"), wxT(""), wxITEM_NORMAL);
	WxPopupMenu1->Append(ID_MNU_YETANOTHERMENUITEM_1005, wxT("Copy path to clipboard..."), wxT(""), wxITEM_NORMAL);
	WxPopupMenu1->Append(ID_MNU_YETANOTHERMENUITEM_1006, wxT("Move file from E: to C:..."), wxT(""), wxITEM_NORMAL);

}
//=======================================================================
//                           scanner thread
//=======================================================================
//currently mostly-disabled.
void *ListFilesThread::Entry()
{
    while(!g_mainFrame->CheckForTermination(200)){
        if(m_rescan){
            //PostCommandEvent(g_mainFrame,EventID_PopulateFilesList);
            m_rescan = false;
        }
    }
    return NULL;
}

// =======================================================================
//                            Event handlers
// =======================================================================

BEGIN_EVENT_TABLE(FilesList, wxListView)
    EVT_KEY_DOWN(FilesList::OnKeyDown)
    EVT_KEY_UP(FilesList::OnKeyUp)
    EVT_LEFT_DCLICK(FilesList::OnMouseLDClick)
    EVT_RIGHT_DOWN(FilesList::OnMouseRClick)
    EVT_LIST_ITEM_SELECTED(wxID_ANY,FilesList::OnSelectionChange)
    EVT_LIST_ITEM_DESELECTED(wxID_ANY,FilesList::OnSelectionChange)
    EVT_MENU(ID_MNU_YETANOTHERMENUITEM_1006,FilesList::RClickMoveFile)
END_EVENT_TABLE()


void FilesList::RClickMoveFile(wxCommandEvent& event)
{
    wxListItem theitem;
    theitem.m_itemId = currentlyselected;
    theitem.m_col = 0;
    theitem.m_mask = wxLIST_MASK_TEXT;
    GetItem(theitem);
    wxString itemtext = theitem.m_text;

    const wchar_t *srcfilename = (const wchar_t *)wcsdup(itemtext.wc_str());
    dtrace("srcfilename was %ws",srcfilename);

    itemtext.Replace(wxT("E:\\"),wxT("C:\\"));
    const wchar_t *dstfilename = (const wchar_t *)wcsdup(itemtext.wc_str());
    dtrace("dstfilename was %ws",dstfilename);

    const wchar_t *dstpath = itemtext.wc_str();
    winx_path_remove_filename((wchar_t *)dstpath);
    dtrace("dst path was %ws",dstpath);

    Utils::createDirectoryRecursively(dstpath);

    CopyFile(srcfilename,dstfilename,1);
    winx_free(srcfilename); //since wcsdup calls malloc
    winx_free(dstfilename); // ^^^
    //works.
}

void FilesList::OnKeyDown(wxKeyEvent& event)
{
    if(!g_mainFrame->m_busy) event.Skip();
}

void FilesList::OnKeyUp(wxKeyEvent& event)
{
    if(!g_mainFrame->m_busy){
/*         dtrace("Modifier: %d ... KeyCode: %d", \
 *             event.GetModifiers(), event.GetKeyCode());
 */
        switch(event.GetKeyCode()){
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
            if(event.GetModifiers() == wxMOD_NONE)
                PostCommandEvent(g_mainFrame,EventID_DefaultAction);
            break;
        case 'A':
            if(event.GetModifiers() == wxMOD_CONTROL)
                PostCommandEvent(g_mainFrame,EventID_SelectAll);
            break;
        default:
            break;
        }
        event.Skip();
    }
}

void FilesList::OnMouseLDClick(wxMouseEvent& event)
{
    if(!g_mainFrame->m_busy){
        // left double click starts default action
        if(event.GetEventType() == wxEVT_LEFT_DCLICK)
            PostCommandEvent(g_mainFrame,EventID_DefaultAction);
    }
    event.Skip();
}

void FilesList::OnMouseRClick(wxMouseEvent& event)
{
    if (currentlyselected != -1){
        //right click brings up popup menu. (but only if something is selected.
        if(event.GetEventType() == wxEVT_RIGHT_DOWN)
            this->PopupMenu(WxPopupMenu1);
    }
    event.Skip();
}

void FilesList::OnSelectionChange(wxListEvent& event)
{
//    wxListItem info;
//    info.m_itemId = event.m_itemIndex;
//    info.m_col = 0;
//    info.m_mask = wxLIST_MASK_TEXT;
//    GetItem(info);
//    dtrace("path was %s",info.m_text.mb_str(wxConvUTF8).data());
    currentlyselected =  event.m_itemIndex;
//    if(currentlyselected != -1){
//        char letter = (char)GetItemText(i)[0];
//        JobsCacheEntry *currentJob = g_mainFrame->m_jobsCache[(int)letter];
//        if(g_mainFrame->m_currentJob != currentJob){
//            g_mainFrame->m_currentJob = currentJob;
//            PostCommandEvent(g_mainFrame,EventID_UpdateStatusBar);
//            PostCommandEvent(g_mainFrame,EventID_FilesAnalyzedUpdateFilesList);
//        }
//    }
//    PostCommandEvent(g_mainFrame,EventID_FilesAnalyzedUpdateFilesList);
    event.Skip();
}

void MainFrame::FilesAdjustListColumns(wxCommandEvent& event)
{
    int width = event.GetInt();
    if(width == 0)
        width = m_filesList->GetClientSize().GetWidth();


    int border = wxSystemSettings::GetMetric(wxSYS_BORDER_Y);

    dtrace("FilesList border width ......... %d", border);
    dtrace("FilesList client width ......... %d", width);

    int firstwidth = width - border*4;
    for(int i = LIST_COLUMNS - 1; i > 0; i--){
        firstwidth -= m_filesList->GetColumnWidth(i);
    }
    m_filesList->SetColumnWidth(0, firstwidth);
    dtrace("column %d width ....... %d", 0, firstwidth);
}

void MainFrame::FilesAdjustListHeight(wxCommandEvent& WXUNUSED(event))
{
    //Commented out, because its not needed.
}
//    // get client height of the list
//    int height = m_filesList->GetClientSize().GetHeight();
//    dtrace("FilesList getclient.getheight  of the list ......... %d", height);
//
//    // avoid recursion
//    if(height == m_filesListHeight)
//        return;
//    m_filesListHeight = height;
//
//    if(!m_filesList->GetColumnCount())
//        return;
//
//    // get height of the list header
//    HWND header = ListView_GetHeader((HWND)m_filesList->GetHandle());
//    if(!header){
//        letrace("cannot get list header");
//        return;
//    }
//
//    RECT rc;
//    if(!Header_GetItemRect(header,0,&rc)){
//        letrace("cannot get list header size");
//        return;
//    }
//    int header_height = rc.bottom - rc.top;
//
//    // get height of a single row
//    wxRect rect;
//    if(!m_filesList->GetItemRect(0,rect))
//        return;
//    int item_height = rect.GetHeight();
//    dtrace("FilesList one itemheight ......... %d", item_height);
//
//    //genBTC
//    int items = m_filesList->GetItemCount();
//    int new_height = items * item_height + header_height;
//    m_filesListHeight = new_height;
//
//    dtrace("FilesList mfileslistheight-end......... %d", m_filesListHeight);
//    // adjust client height of the list
//    new_height += 2 * wxSystemSettings::GetMetric(wxSYS_BORDER_Y);
////    m_splitter->SetSashPosition(new_height);
//}

void MainFrame::FilesOnSplitChanged(wxSplitterEvent& event)
{
    PostCommandEvent(this,EventID_AdjustFilesListHeight);
    PostCommandEvent(this,EventID_AdjustFilesListColumns);

    event.Skip();
}

void MainFrame::FilesOnListSize(wxSizeEvent& event)
{
    int old_width = m_filesList->GetClientSize().GetWidth();
    int new_width = this->GetClientSize().GetWidth();
    new_width -= 2 * wxSystemSettings::GetMetric(wxSYS_EDGE_X);
    if(m_filesList->GetCountPerPage() < m_filesList->GetItemCount())
        new_width -= wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);

    // scale list columns; avoid horizontal scrollbar appearance
    wxCommandEvent evt(wxEVT_COMMAND_MENU_SELECTED,EventID_AdjustFilesListColumns);
    evt.SetInt(new_width);
    if(new_width <= old_width)
        ProcessEvent(evt);
    else
        wxPostEvent(this,evt);

    event.Skip();
}

/**
 * @brief Deletes the files list and re-populates. Should Only called Once.
*/
void MainFrame::FilesPopulateList(wxCommandEvent& event)
{

    struct prb_traverser trav;
    winx_file_info *file;
    wxString comment, status;
    int currentitem = 0;

    if(!&m_jobsCache){
      etrace("FAILED to obtain currentJob CacheEntry!!");
      return;
    }
    //JobsCacheEntry *newEntry = (JobsCacheEntry *)event.GetClientData();
    char letter = (char)event.GetInt();
    JobsCacheEntry cacheEntry = *m_jobsCache[(int)letter];

    if (cacheEntry.pi.completion_status <= 0){
        etrace("For some odd reason, Completion status was NOT complete.");
        return;
    }
    else {
        m_filesList->DeleteAllItems();

        prb_t_init(&trav,cacheEntry.pi.fragmented_files_prb);
        file = (winx_file_info *)prb_t_first(&trav,cacheEntry.pi.fragmented_files_prb);
        if (!file){
            etrace("Fragmented Files List File Not Found.");
            return;
        }

        while (file){

            wxString test((const wchar_t *)(file->path + 4));
            m_filesList->InsertItem(currentitem,test,2);

            wxString numfragments = wxString::Format(wxT("%d"),file->disp.fragments);
            m_filesList->SetItem(currentitem,1,numfragments);

            int bpc = m_volinfocache.bytes_per_cluster;
//            if (bpc > 32768 || bpc < 512){
//                etrace("m_volinfocache was not available!");
//                return;
//            }
            ULONGLONG filesizebytes = file->disp.clusters * bpc;
            char filesize_hr[32];
            winx_bytes_to_hr(filesizebytes,2,filesize_hr,sizeof(filesize_hr));
            wxString filename;
            filename.Printf(wxT("%hs"),filesize_hr);
            m_filesList->SetItem(currentitem,2,filename);

            if(is_directory(file))
                comment = wxT("[DIR]");
            else if(is_compressed(file))
                comment = wxT("Compressed");
            else if(is_essential_boot_file(file))
                comment = wxT("[BOOT]");
            else if(is_mft_file(file))
                comment = wxT("[MFT]");
            else
                comment = wxT("");
            status = wxT("");
            if(is_locked(file))
                status = wxT("Locked");

            m_filesList->SetItem(currentitem,3,comment);
            m_filesList->SetItem(currentitem,4,status);

            // ULONGLONG time is stored in how many of 100 nanoseconds (0.1 microseconds) or (0.0001 milliseconds) or 0.0000001 seconds.
            //WindowsTickToUnixSeconds() (alternate way.)
            winx_time lmt;             //Last Modified time:
            winx_filetime2winxtime(file->last_modification_time,&lmt);

            char lmtbuffer[64];
            (void)_snprintf(lmtbuffer,sizeof(lmtbuffer),
                "%02i/%02i/%04i"
                " "
                "%02i:%02i:%02i",
                (int)lmt.month,(int)lmt.day,(int)lmt.year,
                (int)lmt.hour,(int)lmt.minute,(int)lmt.second
            );
            lmtbuffer[sizeof(lmtbuffer) - 1] = 0; //terminate witha 0.
            wxString lastmodtime;
            lastmodtime.Printf(wxT("%hs"),lmtbuffer);
            m_filesList->SetItem(currentitem,5,lastmodtime);

            currentitem++;

            file = (winx_file_info *)prb_t_next(&trav);
        }
    }
    if (currentitem > 0)
        dtrace("Successfully finished with the Populate List Loop");

    else
        dtrace("Populate List Loop Did not run, no files were added.");
    PostCommandEvent(this,EventID_AdjustFilesListColumns);
    gui_fileslist_finished();
}

void MainFrame::FilesAnalyzedUpdateFilesList (wxCommandEvent& event)
{
    //commented out because its not needed.
}
void MainFrame::FilesOnSkipRem(wxCommandEvent& WXUNUSED(event))
{
    if(!m_busy){
        m_skipRem = m_menuBar->FindItem(EventID_SkipRem)->IsChecked();
        m_listfilesThread->m_rescan = true;
    }
}

void MainFrame::FilesOnRescan(wxCommandEvent& WXUNUSED(event))
{
    if(!m_busy) m_listfilesThread->m_rescan = true;
}


/** @} */
