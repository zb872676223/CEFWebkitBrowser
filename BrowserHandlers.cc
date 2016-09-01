#include "stdafx.h"
#include "BrowserHandlers.h"
#include <wrapper/cef_helpers.h>
#include <base/cef_bind.h>


//TID_UI �߳�������������̡߳����Ӧ�ó����ڵ��õ���CefInitialize()ʱ������CefSettings.multi_threaded_message_loop = false������߳�Ҳ��Ӧ�ó�������̡߳�
//TID_IO �߳���Ҫ������IPC��Ϣ�Լ�����ͨ�š�
//TID_FILE �̸߳������ļ�ϵͳ������


// 1.  User clicks the window close button which sends an OS close
//     notification (e.g. WM_CLOSE on Windows, performClose: on OS-X and
//     "delete_event" on Linux).
// 2.  Application's top-level window receives the close notification and:
//     A. Calls CefBrowserHost::CloseBrowser(false).
//     B. Cancels the window close.
// 3.  JavaScript 'onbeforeunload' handler executes and shows the close
//     confirmation dialog (which can be overridden via
//     CefJSDialogHandler::OnBeforeUnloadDialog()).
// 4.  User approves the close.
// 5.  JavaScript 'onunload' handler executes.
// 6.  Application's DoClose() handler is called. Application will:
//     A. Set a flag to indicate that the next close attempt will be allowed.
//     B. Return false.
// 7.  CEF sends an OS close notification.
// 8.  Application's top-level window receives the OS close notification and
//     allows the window to close based on the flag from #6B.
// 9.  Browser OS window is destroyed.
// 10. Application's CefLifeSpanHandler::OnBeforeClose() handler is called and
//     the browser object is destroyed.
// 11. Application exits by calling CefQuitMessageLoop() if no other browsers
//     exist.
///


CCefClientHandler::CCefClientHandler() :hWnd_(NULL), is_closing_(false)
{
}

CCefClientHandler::~CCefClientHandler()
{

}


// CefClient methods:
CefRefPtr<CefDisplayHandler> CCefClientHandler::GetDisplayHandler()
{
	return this;
}
CefRefPtr<CefLifeSpanHandler> CCefClientHandler::GetLifeSpanHandler()
{
	return this;
}
CefRefPtr<CefLoadHandler> CCefClientHandler::GetLoadHandler()
{
	return this;
}

CefRefPtr<CefRequestHandler> CCefClientHandler::GetRequestHandler()
{
	return this;
}

// ����һ��ָ��browser������
void CCefClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	// Add to the list of existing browsers.
	browser_list_.push_back(browser);

	int nID = browser->GetIdentifier();

	::PostMessage(hWnd_, UM_CEF_AFTERCREATED, nID, 0);

}


bool CCefClientHandler::DoClose(CefRefPtr<CefBrowser> browser)
{
	//TID_UI �߳�������������̡߳�
	CEF_REQUIRE_UI_THREAD();
//	AutoLock lock_scope(this);

	lock_.Acquire();


	// Remove from the list of existing browsers.
	BrowserList::iterator bit = browser_list_.begin();

	for (; bit != browser_list_.end(); bit++)
	{
		if ((*bit)->IsSame(browser))
		{
			browser_list_.erase(bit);
			browser = NULL;
			break;
		}
	}


	lock_.Release();



	if (browser_list_.size() == 0) {
		// Set a flag to indicate that the window close should be allowed.
		is_closing_ = true;
	
		::PostMessage(hWnd_, UM_CEF_POSTQUITMESSAGE, 0, 0);
	}

	
	return false;
}


void CCefClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	CEF_REQUIRE_UI_THREAD();

	AutoLock lock_scope(this);
//	lock_.Acquire();

	BrowserList::iterator bit = browser_list_.begin();

	for (; bit != browser_list_.end(); bit++)
	{
		if ((*bit)->IsSame(browser))
		{
			browser_list_.erase(bit);
			browser = NULL;
			break;
		}
	}


	if (browser_list_.empty()) 
	{
		is_closing_ = true;
		::PostMessage(hWnd_, UM_CEF_POSTQUITMESSAGE, 0, 0);
		// All browser windows have closed. Quit the application message loop.
		//CefQuitMessageLoop();
		//PostQuitMessage(0l);
	}

//	lock_.Release();
}

void CCefClientHandler::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame)
{
	CEF_REQUIRE_UI_THREAD();

	CefString* strTmpURL = new CefString(browser->GetMainFrame()->GetURL());
	int nID = browser->GetIdentifier();
	::PostMessage(hWnd_, UM_CEF_WEBLOADSTART, nID, (LPARAM)strTmpURL);
	
	//return __super::OnLoadStart(browser, frame);
}
void CCefClientHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode)
{
	CEF_REQUIRE_UI_THREAD();

	CefString* strTmpURL = new CefString(browser->GetMainFrame()->GetURL());
	int nID = browser->GetIdentifier();
	::PostMessage(hWnd_, UM_CEF_WEBLOADEND, nID, (LPARAM)strTmpURL);

}



void CCefClientHandler::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl)
{
	CEF_REQUIRE_UI_THREAD();

	// Don't display an error for downloaded files.
	if (errorCode == ERR_ABORTED)
		return;

	// Display a load error message.
	std::stringstream ss;
	ss << "<html><body bgcolor=\"white\">"
		"<h2>Failed to load URL " << std::string(failedUrl) <<
		" with error " << std::string(errorText) << " (" << errorCode <<
		").</h2></body></html>";
	frame->LoadString(ss.str(), failedUrl);
}





void CCefClientHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title)
{
	CEF_REQUIRE_UI_THREAD();

	// globally unique identifier for this browser
	int nID=browser->GetIdentifier();

	CefString* strTitle = new CefString(title);
	::PostMessage(hWnd_, UM_CEF_WEBTITLECHANGE, nID, (LPARAM)strTitle);
}



bool CCefClientHandler::OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& target_url, const CefString& target_frame_name, CefLifeSpanHandler::WindowOpenDisposition target_disposition,
	bool user_gesture, const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo, CefRefPtr<CefClient>& client, CefBrowserSettings& settings, bool* no_javascript_access)
{

	//��Ϣ������ǵ�delete������ַ�����Դ
	CefString* strTargetURL = new CefString(target_url);
	::PostMessage(hWnd_, UM_CEF_WEBLOADPOPUP, (WPARAM)0, (LPARAM)strTargetURL);
	return true;

}

void CCefClientHandler::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model)
{
	//��������������Լ���Ҫ�Ĳ˵�  
	cef_context_menu_type_flags_t flag = params->GetTypeFlags();
	if (flag & CM_TYPEFLAG_PAGE)
	{//��ͨҳ����Ҽ���Ϣ  
		model->SetLabel(MENU_ID_BACK, L"����");
		model->SetLabel(MENU_ID_FORWARD, L"ǰ��");
		model->SetLabel(MENU_ID_VIEW_SOURCE, L"�鿴Դ����");
		model->SetLabel(MENU_ID_PRINT, L"��ӡ");
		model->SetLabel(MENU_ID_RELOAD, L"ˢ��");
		model->SetLabel(MENU_ID_RELOAD_NOCACHE, L"ǿ��ˢ��");
		model->SetLabel(MENU_ID_STOPLOAD, L"ֹͣ����");
		model->SetLabel(MENU_ID_REDO, L"�ظ�");
	
		
	}
	if (flag & CM_TYPEFLAG_EDITABLE)
	{//�༭����Ҽ���Ϣ  
		model->SetLabel(MENU_ID_UNDO, L"����");
		model->SetLabel(MENU_ID_REDO, L"����");
		model->SetLabel(MENU_ID_CUT, L"����");
		model->SetLabel(MENU_ID_COPY, L"����");
		model->SetLabel(MENU_ID_PASTE, L"ճ��");
		model->SetLabel(MENU_ID_DELETE, L"ɾ��");
		model->SetLabel(MENU_ID_SELECT_ALL, L"ȫѡ");
	}
}

bool CCefClientHandler::OnContextMenuCommand(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,CefRefPtr<CefContextMenuParams> params, int command_id, EventFlags event_flags)
{
	return false;
}



void CCefClientHandler::CloseHostBrowser(CefRefPtr<CefBrowser>browser, bool force_close)
{

	if (!CefCurrentlyOn(TID_UI))
	{
		// Execute on the UI thread.
		CefPostTask(TID_UI, base::Bind(&CCefClientHandler::CloseHostBrowser, this,browser, force_close));
		return;
	}


	int nID = browser->GetIdentifier();

	::PostMessage(hWnd_, UM_CEF_BROWSERCLOSE, nID, 0);

	browser->GetHost()->CloseBrowser(force_close);
}


void CCefClientHandler::CloseAllBrowsers(bool force_close)
{
	//if (!CefCurrentlyOn(TID_UI))
	//{
	//	// Execute on the UI thread.
	//	CefPostTask(TID_UI, base::Bind(&CCefClientHandler::CloseAllBrowsers, this, force_close));
	//	return;
	//}



	lock_.Acquire();


	if (browser_list_.empty())
		return;

	BrowserList::const_iterator it = browser_list_.begin();
	for (; it != browser_list_.end(); ++it)
	{
		(*it)->GetHost()->CloseBrowser(force_close);
		
	}

	lock_.Release();
	
}


bool CCefClientHandler::IsClosing() const
{ 
	return is_closing_; 
}

