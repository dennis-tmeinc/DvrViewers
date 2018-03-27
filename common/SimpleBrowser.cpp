// SimpleBrowser.cpp
// 
//	This is C++ rewrite of code from "Embed an HTML control in your own window using plain C"
//		https://www.codeproject.com/articles/3365/embed-an-html-control-in-your-own-window-using-pla
//
//	Added IDispatch interface for external, so javascript could communicate with C++ code ^^
//
#include "stdafx.h"

#include <Windows.h>
#include <exdisp.h>		// Defines of stuff like IWebBrowser2. This is an include file with Visual C 6 and above
#include <mshtml.h>		// Defines of stuff like IHTMLDocument2. This is an include file with Visual C 6 and above
#include <mshtmhst.h>	// Defines of stuff like IDocHostUIHandler. This is an include file with Visual C 6 and above
#include <crtdbg.h>		// for _ASSERT()
#include <tchar.h>		// for _tcsnicmp which compares two UNICODE strings case-insensitive

// This is a simple C example. There are lots more things you can control about the browser object, but
// we don't do it in this example. _Many_ of the functions we provide below for the browser to call, will
// never actually be called by the browser in our example. Why? Because we don't do certain things
// with the browser that would require it to call those functions (even though we need to provide
// at least some stub for all of the functions).
//
// So, for these "dummy functions" that we don't expect the browser to call, we'll just stick in some
// assembly code that causes a debugger breakpoint and tells the browser object that we don't support
// the functionality. That way, if you try to do more things with the browser object, and it starts
// calling these "dummy functions", you'll know which ones you should add more meaningful code to.
#define NOTIMPLEMENTED _ASSERT(0); return(E_NOTIMPL)

// help to get IOleClientSiteEx pointer
static size_t offset_inplace();
static size_t offset_frame();
static size_t offset_ui();
static size_t offset_external();
static size_t offset_browser();
static size_t offset_window();

// We need to return an IOleInPlaceFrame struct to the browser object. And one of our IOleInPlaceFrame
// functions (Frame_GetWindow) is going to need to access our window handle. So let's create our own
// struct that starts off with an IOleInPlaceFrame struct (and that's important -- the IOleInPlaceFrame
// struct *must* be first), and then has an extra data field where we can store our own window's HWND.
//
// And because we may want to create multiple windows, each hosting its own browser object (to
// display its own web page), then we need to create a IOleInPlaceFrame struct for each window. So,
// we're not going to declare our IOleInPlaceFrame struct globally. We'll allocate it later using
// GlobalAlloc, and then stuff the appropriate HWND in it then, and also stuff a pointer to
// MyIOleInPlaceFrameTable in it. But let's just define it here.

struct IOleInPlaceFrameEx : public IOleInPlaceFrame 
{
public:

	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
	{
		*ppvObject = NULL;
		NOTIMPLEMENTED;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef(void) {
		return(1);
	}

	virtual ULONG STDMETHODCALLTYPE Release(void)
	{
		return(1);
	};


	// IOleWindow : public IUnknown
	virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE GetWindow(
		/* [out] */ __RPC__deref_out_opt HWND *phwnd)
	{
		// Give the browser the HWND to our window that contains the browser object. 
		*phwnd = *(HWND *)((char *)this - offset_frame() + offset_window());
		return(S_OK);
	}

	virtual HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(
		/* [in] */ BOOL fEnterMode) 
	{
		NOTIMPLEMENTED;
	}

	// IOleInPlaceUIWindow
	virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE GetBorder(
		/* [out] */ __RPC__out LPRECT lprectBorder) {
		NOTIMPLEMENTED;
	}

	virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE RequestBorderSpace(
		/* [unique][in] */ __RPC__in_opt LPCBORDERWIDTHS pborderwidths) 
	{
		NOTIMPLEMENTED;
	}

	virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE SetBorderSpace(
		/* [unique][in] */ __RPC__in_opt LPCBORDERWIDTHS pborderwidths) 
	{
		NOTIMPLEMENTED;
	}

	virtual HRESULT STDMETHODCALLTYPE SetActiveObject(
		/* [unique][in] */ __RPC__in_opt IOleInPlaceActiveObject *pActiveObject,
		/* [unique][string][in] */ __RPC__in_opt_string LPCOLESTR pszObjName) 
	{
		return(S_OK);
	}


	// IOleInPlaceFrame
	virtual HRESULT STDMETHODCALLTYPE InsertMenus(
		/* [in] */ __RPC__in HMENU hmenuShared,
		/* [out][in] */ __RPC__inout LPOLEMENUGROUPWIDTHS lpMenuWidths) 
	{
		NOTIMPLEMENTED;
	};

	virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE SetMenu(
		/* [in] */ __RPC__in HMENU hmenuShared,
		/* [in] */ __RPC__in HOLEMENU holemenu,
		/* [in] */ __RPC__in HWND hwndActiveObject) 
	{
		return(S_OK);
	}

	virtual HRESULT STDMETHODCALLTYPE RemoveMenus(
		/* [in] */ __RPC__in HMENU hmenuShared) 
	{
		NOTIMPLEMENTED;
	}

	virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE SetStatusText(
		/* [unique][in] */ __RPC__in_opt LPCOLESTR pszStatusText) 
	{
		return(S_OK);
	}

	virtual HRESULT STDMETHODCALLTYPE EnableModeless(
		/* [in] */ BOOL fEnable) 
	{
		return(S_OK);
	}

	virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(
		/* [in] */ __RPC__in LPMSG lpmsg,
		/* [in] */ WORD wID)
	{
		NOTIMPLEMENTED;
	}

};



struct IOleInPlaceSite_Ex : public IOleInPlaceSite
{
public:

	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
	{

		if (riid == IID_IOleInPlaceSite || riid == IID_IOleWindow || riid == IID_IUnknown) {
			*ppvObject = this;
			return S_OK;
		}

		// The browser assumes that our IOleInPlaceSite object is associated with our IOleClientSite
		// object. So it is possible that the browser may call our IOleInPlaceSite's QueryInterface()
		// to ask us to return a pointer to our IOleClientSite, in the same way that the browser calls
		// our IOleClientSite's QueryInterface() to ask for a pointer to our IOleInPlaceSite.
		//
		// Rather than duplicate much of the code in IOleClientSite's QueryInterface, let's just get
		// a pointer to our _IOleClientSiteEx object, substitute it as the 'This' arg, and call our
		// our IOleClientSite's QueryInterface. Note that since our IOleInPlaceSite is embedded right
		// inside our _IOleClientSiteEx, and comes immediately after the IOleClientSite, we can employ
		// the following trickery to get the pointer to our _IOleClientSiteEx.
		IOleClientSite * iOleClientSite;
		iOleClientSite = (IOleClientSite *)((char *)this - offset_inplace());
		return iOleClientSite->QueryInterface(riid, ppvObject);
	}

	virtual ULONG STDMETHODCALLTYPE AddRef(void) {
		return(1);
	}

	virtual ULONG STDMETHODCALLTYPE Release(void)
	{
		return(1);
	};

	// IOleWindow
	virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE GetWindow(
		/* [out] */ __RPC__deref_out_opt HWND *phwnd) 
	{
		// Return the HWND of the window that contains this browser object.
		*phwnd = *(HWND *)((char *)this - offset_inplace() + offset_window());
		return(S_OK);
	}

	virtual HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(
		/* [in] */ BOOL fEnterMode) 
	{
		NOTIMPLEMENTED;
	}

	// IOleInPlaceSite

	virtual HRESULT STDMETHODCALLTYPE CanInPlaceActivate(void) 
	{
		// Tell the browser we can in place activate
		return(S_OK);
	}

	virtual HRESULT STDMETHODCALLTYPE OnInPlaceActivate(void)
	{
		// Tell the browser we did it ok
		return(S_OK);
	}

	virtual HRESULT STDMETHODCALLTYPE OnUIActivate(void)
	{
		return(S_OK);
	}

	virtual HRESULT STDMETHODCALLTYPE GetWindowContext(
		/* [out] */ __RPC__deref_out_opt IOleInPlaceFrame **ppFrame,
		/* [out] */ __RPC__deref_out_opt IOleInPlaceUIWindow **ppDoc,
		/* [out] */ __RPC__out LPRECT lprcPosRect,
		/* [out] */ __RPC__out LPRECT lprcClipRect,
		/* [out][in] */ __RPC__inout LPOLEINPLACEFRAMEINFO lpFrameInfo)
	{
		// Give the browser the pointer to our IOleInPlaceFrame struct. We stored that pointer
		// in our _IOleInPlaceSiteEx struct. Nevermind that the function declaration for
		// Site_GetWindowContext says that 'This' is an IOleInPlaceSite *. Remember that in
		// EmbedBrowserObject(), we allocated our own _IOleInPlaceSiteEx struct which
		// contained an embedded IOleInPlaceSite struct within it. And when the browser
		// called Site_QueryInterface() to get a pointer to our IOleInPlaceSite object, we
		// returned a pointer to our _IOleClientSiteEx. The browser doesn't know this. But
		// we do. That's what we're really being passed, so we can recast it and use it as
		// so here.
		//
		// Actually, we're giving the browser a pointer to our own _IOleInPlaceSiteEx struct,
		// but telling the browser that it's a IOleInPlaceSite struct. No problem. Our
		// _IOleInPlaceSiteEx starts with an embedded IOleInPlaceSite, so the browser is
		// cool with it. And we want the browser to pass a pointer to this _IOleInPlaceSiteEx
		// wherever it would pass a IOleInPlaceSite struct to our IOleInPlaceSite functions.
		*ppFrame = (IOleInPlaceFrame *)((char *)this - offset_inplace() + offset_frame());

		// We have no OLEINPLACEUIWINDOW
		*ppDoc = NULL;

		// Fill in some other info for the browser
		lpFrameInfo->fMDIApp = FALSE;
		lpFrameInfo->hwndFrame = * (HWND *)((char *)this - offset_inplace() + offset_window());
		lpFrameInfo->haccel = 0;
		lpFrameInfo->cAccelEntries = 0;

		// Give the browser the dimensions of where it can draw. We give it our entire window to fill.
		// We do this in InPlace_OnPosRectChange() which is called right when a window is first
		// created anyway, so no need to duplicate it here.
		//	GetClientRect(lpFrameInfo->hwndFrame, lprcPosRect);
		//	GetClientRect(lpFrameInfo->hwndFrame, lprcClipRect);

		return(S_OK);
	}


	virtual HRESULT STDMETHODCALLTYPE Scroll(
		/* [in] */ SIZE scrollExtant) 
	{
		NOTIMPLEMENTED;
	}

	virtual HRESULT STDMETHODCALLTYPE OnUIDeactivate(
		/* [in] */ BOOL fUndoable) 
	{
		return(S_OK);
	}

	virtual HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate(void)
	{
		return(S_OK);
	}


	virtual HRESULT STDMETHODCALLTYPE DiscardUndoState(void)
	{
		NOTIMPLEMENTED;
	}

	virtual HRESULT STDMETHODCALLTYPE DeactivateAndUndo(void)
	{
		NOTIMPLEMENTED;
	}

	// Called when the position of the browser object is changed, such as when we call the IWebBrowser2's put_Width(),
	// put_Height(), put_Left(), or put_Right().
	virtual HRESULT STDMETHODCALLTYPE OnPosRectChange(
		/* [in] */ __RPC__in LPCRECT lprcPosRect) 
	{
		IOleInPlaceObject	*inplace;
		IOleObject	*browserObject;		

		// We need to get the browser's IOleInPlaceObject object so we can call its SetObjectRects
		// function.
		browserObject = *(IOleObject	**)((char *)this - offset_inplace() + offset_browser());
		if (browserObject && !browserObject->QueryInterface(IID_IOleInPlaceObject, (void**)&inplace))
		{
			// Give the browser the dimensions of where it can draw.
			inplace->SetObjectRects(lprcPosRect, lprcPosRect);
			inplace->Release();
		}

		return(S_OK);
	}

};




struct IDocHostUIHandlerEx : public IDocHostUIHandler 
{
public:
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) {


		if (riid == IID_IDocHostUIHandler || riid == IID_IUnknown) {
			*ppvObject = this;
			return S_OK;
		}

		// The browser assumes that our IDocHostUIHandler object is associated with our IOleClientSite
		// object. So it is possible that the browser may call our IDocHostUIHandler's QueryInterface()
		// to ask us to return a pointer to our IOleClientSite, in the same way that the browser calls
		// our IOleClientSite's QueryInterface() to ask for a pointer to our IDocHostUIHandler.
		//
		// Rather than duplicate much of the code in IOleClientSite's QueryInterface, let's just get
		// a pointer to our _IOleClientSiteEx object, substitute it as the 'This' arg, and call our
		// our IOleClientSite's QueryInterface. Note that since our _IDocHostUIHandlerEx is embedded right
		// inside our _IOleClientSiteEx, and comes immediately after the _IOleInPlaceSiteEx, we can employ
		// the following trickery to get the pointer to our _IOleClientSiteEx.
		IOleClientSite * iOleClientSite;
		iOleClientSite = (IOleClientSite *)((char *)this - offset_ui());
		return iOleClientSite->QueryInterface(riid, ppvObject);
	}

	virtual ULONG STDMETHODCALLTYPE AddRef(void) {
		return(1);
	}

	virtual ULONG STDMETHODCALLTYPE Release(void)
	{
		return(1);
	}

	// IDocHostUIHandler

	// Called when the browser object is about to display its context menu.
	virtual HRESULT STDMETHODCALLTYPE ShowContextMenu(
		/* [annotation][in] */
		_In_  DWORD dwID,
		/* [annotation][in] */
		_In_  POINT *ppt,
		/* [annotation][in] */
		_In_  IUnknown *pcmdtReserved,
		/* [annotation][in] */
		_In_  IDispatch *pdispReserved) 
	{
		// If desired, we can pop up your own custom context menu here. Of course,
		// we would need some way to get our window handle, so what we'd probably
		// do is like what we did with our IOleInPlaceFrame object. We'd define and create
		// our own IDocHostUIHandlerEx object with an embedded IDocHostUIHandler at the
		// start of it. Then we'd add an extra HWND field to store our window handle.
		// It could look like this:
		//
		// typedef struct _IDocHostUIHandlerEx {
		//		IDocHostUIHandler	ui;		// The IDocHostUIHandler must be first!
		//		HWND				window;
		// } IDocHostUIHandlerEx;
		//
		// Of course, we'd need a unique IDocHostUIHandlerEx object for each window, so
		// EmbedBrowserObject() would have to allocate one of those too. And that's
		// what we'd pass to our browser object (and it would in turn pass it to us
		// here, instead of 'This' being a IDocHostUIHandler *).

		// We will return S_OK to tell the browser not to display its default context menu,
		// or return S_FALSE to let the browser show its default context menu. For this
		// example, we wish to disable the browser's default context menu.
		return(S_OK);
	}

	// Called at initialization of the browser object UI. We can set various features of the browser object here.
	virtual HRESULT STDMETHODCALLTYPE GetHostInfo(
		/* [annotation][out][in] */
		_Inout_  DOCHOSTUIINFO *pInfo) 
	{
		pInfo->cbSize = sizeof(DOCHOSTUIINFO);

		// Set some flags. We don't want any 3D border. You can do other things like hide
		// the scroll bar (DOCHOSTUIFLAG_SCROLL_NO), display picture display (DOCHOSTUIFLAG_NOPICS),
		// disable any script running when the page is loaded (DOCHOSTUIFLAG_DISABLE_SCRIPT_INACTIVE),
		// open a site in a new browser window when the user clicks on some link (DOCHOSTUIFLAG_OPENNEWWIN),
		// and lots of other things. See the MSDN docs on the DOCHOSTUIINFO struct passed to us.
		pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER;

		// Set what happens when the user double-clicks on the object. Here we use the default.
		pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;

		return(S_OK);
	}

	// Called when the browser object shows its UI. This allows us to replace its menus and toolbars by creating our
	// own and displaying them here.
	virtual HRESULT STDMETHODCALLTYPE ShowUI(
		/* [annotation][in] */
		_In_  DWORD dwID,
		/* [annotation][in] */
		_In_  IOleInPlaceActiveObject *pActiveObject,
		/* [annotation][in] */
		_In_  IOleCommandTarget *pCommandTarget,
		/* [annotation][in] */
		_In_  IOleInPlaceFrame *pFrame,
		/* [annotation][in] */
		_In_  IOleInPlaceUIWindow *pDoc) 
	{
		// We've already got our own UI in place so just return S_OK to tell the browser
		// not to display its menus/toolbars. Otherwise we'd return S_FALSE to let it do
		// that.
		return(S_OK);
	}

	// Called when browser object hides its UI. This allows us to hide any menus/toolbars we created in ShowUI.
	virtual HRESULT STDMETHODCALLTYPE HideUI(void) 
	{
		return(S_OK);
	}

	// Called when the browser object wants to notify us that the command state has changed. We should update any
	// controls we have that are dependent upon our embedded object, such as "Back", "Forward", "Stop", or "Home"
	// buttons.
	virtual HRESULT STDMETHODCALLTYPE UpdateUI(void) 
	{
		// We update our UI in our window message loop so we don't do anything here.
		return(S_OK);
	}



	// Called from the browser object's IOleInPlaceActiveObject object's EnableModeless() function. Also
	// called when the browser displays a modal dialog box.
	virtual HRESULT STDMETHODCALLTYPE EnableModeless(
		/* [in] */ BOOL fEnable)
	{
		return(S_OK);
	}


	// Called from the browser object's IOleInPlaceActiveObject object's OnDocWindowActivate() function.
	// This informs off of when the object is getting/losing the focus.
	virtual HRESULT STDMETHODCALLTYPE OnDocWindowActivate(
		/* [in] */ BOOL fActivate)
	{
		return(S_OK);
	}


	// Called from the browser object's IOleInPlaceActiveObject object's OnFrameWindowActivate() function.
	virtual HRESULT STDMETHODCALLTYPE OnFrameWindowActivate(
		/* [in] */ BOOL fActivate) 
	{
		return(S_OK);
	}

	// Called from the browser object's IOleInPlaceActiveObject object's ResizeBorder() function.
	virtual HRESULT STDMETHODCALLTYPE ResizeBorder(
		/* [annotation][in] */
		_In_  LPCRECT prcBorder,
		/* [annotation][in] */
		_In_  IOleInPlaceUIWindow *pUIWindow,
		/* [annotation][in] */
		_In_  BOOL fRameWindow) 
	{
		return(S_OK);
	}

	// Called from the browser object's TranslateAccelerator routines to translate key strokes to commands.
	virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(
		/* [in] */ LPMSG lpMsg,
		/* [in] */ const GUID *pguidCmdGroup,
		/* [in] */ DWORD nCmdID)
	{
		// We don't intercept any keystrokes, so we do nothing here. But for example, if we wanted to
		// override the TAB key, perhaps do something with it ourselves, and then tell the browser
		// not to do anything with this keystroke, we'd do:
		//
		//	if (pMsg && pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
		//	{
		//		// Here we do something as a result of a TAB key press.
		//
		//		// Tell the browser not to do anything with it.
		//		return(S_FALSE);
		//	}
		//
		//	// Otherwise, let the browser do something with this message.
		//	return(S_OK);

		// For our example, we want to make sure that the user can invoke some key to popup the context
		// menu, so we'll tell it to ignore all messages.
		return(S_FALSE);
	}

	// Called by the browser object to find where the host wishes the browser to get its options in the registry.
	// We can use this to prevent the browser from using its default settings in the registry, by telling it to use
	// some other registry key we've setup with the options we want.
	virtual HRESULT STDMETHODCALLTYPE GetOptionKeyPath(
		/* [annotation][out] */
		_Out_  LPOLESTR *pchKey,
		/* [in] */ DWORD dw) 
	{
		// Let the browser use its default registry settings.
		return(S_FALSE);
	}


	// Called by the browser object when it is used as a drop target. We can supply our own IDropTarget object,
	// IDropTarget functions, and IDropTarget VTable if we want to determine what happens when someone drags and
	// drops some object on our embedded browser object.
	virtual HRESULT STDMETHODCALLTYPE GetDropTarget(
		/* [annotation][in] */
		_In_  IDropTarget *pDropTarget,
		/* [annotation][out] */
		_Outptr_  IDropTarget **ppDropTarget) 
	{
		// Return our IDropTarget object associated with this IDocHostUIHandler object. I don't
		// know why we don't do this via UI_QueryInterface(), but we don't.

		// NOTE: If we want/need an IDropTarget interface, then we would have had to setup our own
		// IDropTarget functions, IDropTarget VTable, and create an IDropTarget object. We'd want to put
		// a pointer to the IDropTarget object in our own custom IDocHostUIHandlerEx object (like how
		// we may add an HWND field for the use of UI_ShowContextMenu). So when we created our
		// IDocHostUIHandlerEx object, maybe we'd add a 'idrop' field to the end of it, and
		// store a pointer to our IDropTarget object there. Then we could return this pointer as so:
		//
		// *pDropTarget = ((IDocHostUIHandlerEx *)This)->idrop;
		// return(S_OK);

		// But for our purposes, we don't need an IDropTarget object, so we'll tell whomever is calling
		// us that we don't have one.
		return(S_FALSE);
	}

	// Called by the browser when it wants a pointer to our IDispatch object. This object allows us to expose
	// our own automation interface (ie, our own COM objects) to other entities that are running within the
	// context of the browser so they can call our functions if they want. An example could be a javascript
	// running in the URL we display could call our IDispatch functions. We'd write them so that any args passed
	// to them would use the generic datatypes like a BSTR for utmost flexibility.
	virtual HRESULT STDMETHODCALLTYPE GetExternal(
		/* [annotation][out] */
		_Outptr_result_maybenull_  IDispatch **ppDispatch) 
	{
		// Return our IDispatch object associated with this IDocHostUIHandler object. I don't
		// know why we don't do this via UI_QueryInterface(), but we don't.

		// NOTE: If we want/need an IDispatch interface, then we would have had to setup our own
		// IDispatch functions, IDispatch VTable, and create an IDispatch object. We'd want to put
		// a pointer to the IDispatch object in our custom _IDocHostUIHandlerEx object (like how
		// we may add an HWND field for the use of UI_ShowContextMenu). So when we defined our
		// _IDocHostUIHandlerEx object, maybe we'd add a 'idispatch' field to the end of it, and
		// store a pointer to our IDispatch object there. Then we could return this pointer as so:
		//
		// *ppDispatch = ((_IDocHostUIHandlerEx *)This)->idispatch;
		// return(S_OK);

		// But for our purposes, we don't need an IDispatch object, so we'll tell whomever is calling
		// us that we don't have one. Note: We must set ppDispatch to 0 if we don't return our own
		// IDispatch object.
		*ppDispatch = (IDispatch *)((char *)this - offset_ui() + offset_external() );
		return(S_OK);
	}

	// Called by the browser object to give us an opportunity to modify the URL to be loaded.
	virtual HRESULT STDMETHODCALLTYPE TranslateUrl(
		/* [in] */ DWORD dwTranslate,
		/* [annotation][in] */
		_In_  LPWSTR pchURLIn,
		/* [annotation][out] */
		_Outptr_  LPWSTR *ppchURLOut) 
	{
		// We don't need to modify the URL. Note: We need to set ppchURLOut to 0 if we don't
		// return an OLECHAR (buffer) containing a modified version of pchURLIn.
		*ppchURLOut = 0;
		return(S_FALSE);
	}



	// Called by the browser when it does cut/paste to the clipboard. This allows us to block certain clipboard
	// formats or support additional clipboard formats.
	virtual HRESULT STDMETHODCALLTYPE FilterDataObject(
		/* [annotation][in] */
		_In_  IDataObject *pDO,
		/* [annotation][out] */
		_Outptr_result_maybenull_  IDataObject **ppDORet) 
	{
		// Return our IDataObject object associated with this IDocHostUIHandler object. I don't
		// know why we don't do this via UI_QueryInterface(), but we don't.

		// NOTE: If we want/need an IDataObject interface, then we would have had to setup our own
		// IDataObject functions, IDataObject VTable, and create an IDataObject object. We'd want to put
		// a pointer to the IDataObject object in our custom _IDocHostUIHandlerEx object (like how
		// we may add an HWND field for the use of UI_ShowContextMenu). So when we defined our
		// _IDocHostUIHandlerEx object, maybe we'd add a 'idata' field to the end of it, and
		// store a pointer to our IDataObject object there. Then we could return this pointer as so:
		//
		// *ppDORet = ((_IDocHostUIHandlerEx *)This)->idata;
		// return(S_OK);

		// But for our purposes, we don't need an IDataObject object, so we'll tell whomever is calling
		// us that we don't have one. Note: We must set ppDORet to 0 if we don't return our own
		// IDataObject object.
		*ppDORet = 0;
		return(S_FALSE);
	}

};

#ifndef __EXTERNEL_PARAM__
#define __EXTERNEL_PARAM__
struct external_param {
	DISPID dispIdMember;
	DISPPARAMS *pDispParams;
	VARIANT *pVarResult;
};
#endif	// __EXTERNEL_PARAM__

struct IExternal : public IDispatch {

public:

	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
	{
		if (riid == IID_IDispatch || riid == IID_IUnknown ) {
			*ppvObject = this;
			return S_OK;
		}

		// let IOleClientSite handle other queries 
		IOleClientSite * iOleClientSite;
		iOleClientSite = (IOleClientSite *)((char *)this - offset_external());
		return iOleClientSite->QueryInterface(riid, ppvObject);
	}

	virtual ULONG STDMETHODCALLTYPE AddRef(void) {
		return(1);
	}

	virtual ULONG STDMETHODCALLTYPE Release(void)
	{
		return(1);
	};

	// IDispatch
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(
		/* [out] */ __RPC__out UINT *pctinfo) 
	{
		NOTIMPLEMENTED;
	}

	virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(
		/* [in] */ UINT iTInfo,
		/* [in] */ LCID lcid,
		/* [out] */ __RPC__deref_out_opt ITypeInfo **ppTInfo) 
	{
		NOTIMPLEMENTED;
	}

	virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(
		/* [in] */ __RPC__in REFIID riid,
		/* [size_is][in] */ __RPC__in_ecount_full(cNames) LPOLESTR *rgszNames,
		/* [range][in] */ __RPC__in_range(0, 16384) UINT cNames,
		/* [in] */ LCID lcid,
		/* [size_is][out] */ __RPC__out_ecount_full(cNames) DISPID *rgDispId) 
	{
		HWND hwnd;
		hwnd = *(HWND *)((char *)this - offset_external() + offset_window());

		UINT i;
		for (i = 0; i < cNames; i++) {
			rgDispId[i] = SendMessage(hwnd, WM_APP + 100, (WPARAM)rgszNames[i], NULL );
		}
		return S_OK;
	}

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE Invoke(
		/* [annotation][in] */
		_In_  DISPID dispIdMember,
		/* [annotation][in] */
		_In_  REFIID riid,
		/* [annotation][in] */
		_In_  LCID lcid,
		/* [annotation][in] */
		_In_  WORD wFlags,
		/* [annotation][out][in] */
		_In_  DISPPARAMS *pDispParams,
		/* [annotation][out] */
		_Out_opt_  VARIANT *pVarResult,
		/* [annotation][out] */
		_Out_opt_  EXCEPINFO *pExcepInfo,
		/* [annotation][out] */
		_Out_opt_  UINT *puArgErr)
	{
		HWND hwnd;
		UINT msg = 0 ;
		struct external_param external;

		if (wFlags & DISPATCH_METHOD) {		// method call
			msg = WM_APP + 101;
		}
		else if(wFlags & DISPATCH_PROPERTYGET) {	// property get
			msg = WM_APP + 102;
		}
		else if (wFlags & DISPATCH_PROPERTYPUT) {	// property put
			msg = WM_APP + 103;
		}
		if (msg) {
			hwnd = *(HWND *)((char *)this - offset_external() + offset_window());
			external.dispIdMember = dispIdMember;
			external.pDispParams = pDispParams;
			external.pVarResult = pVarResult;
			if (SendMessage(hwnd, msg, (WPARAM)&external, (LPARAM)NULL)) {
				return S_OK;
			}
		}
		return DISP_E_MEMBERNOTFOUND;
	}


};


// We need to pass our IOleClientSite structure to OleCreate (which in turn gives it to the browser).
// But the browser is also going to ask our IOleClientSite's QueryInterface() to return a pointer to
// our IOleInPlaceSite and/or IDocHostUIHandler structs. So we'll need to have those pointers handy.
// Plus, some of our IOleClientSite and IOleInPlaceSite functions will need to have the HWND to our
// window, and also a pointer to our IOleInPlaceFrame struct. So let's create a single struct that
// has the IOleClientSite, IOleInPlaceSite, IDocHostUIHandler, and IOleInPlaceFrame structs all inside
// it (so we can easily get a pointer to any one from any of those structs' functions). As long as the
// IOleClientSite struct is the very first thing in this custom struct, it's all ok. We can still pass
// it to OleCreate() and pretend that it's an ordinary IOleClientSite. We'll call this new struct a
// _IOleClientSiteEx.
//
// And because we may want to create multiple windows, each hosting its own browser object (to
// display its own web page), then we need to create a unique _IOleClientSiteEx struct for
// each window. So, we're not going to declare this struct globally. We'll allocate it later
// using GlobalAlloc, and then initialize the structs within it.

struct IOleClientSiteEx : public IOleClientSite
{
public:
	IOleInPlaceSite_Ex	inplace;	// My IOleInPlaceSite object. A convenient place to put it.
	IOleInPlaceFrameEx	frame;		// My IOleInPlaceFrame object. 
	IDocHostUIHandlerEx	ui;			// My IDocHostUIHandler object. 
	IExternal			ext;		// My IExternal object
	IOleObject	*browserObject;		// IOleInPlaceSite::OnPosRectChange() use this browserObject 
	HWND				window;

	IOleClientSiteEx() {
		browserObject = NULL;
		window = NULL;
	}

	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
	{
		// It just so happens that the first arg passed to us is our _IOleClientSiteEx struct we allocated
		// and passed to DoVerb() and OleCreate(). 
		//
		// If the browser is asking us to match IID_IOleClientSite, then it wants us to return a pointer to
		// our IOleClientSite struct. Then the browser will use the VTable in that struct to call our
		// IOleClientSite functions. It will also pass this same pointer to all of our IOleClientSite
		// functions.
		//
		// The IUnknown interface uses the same VTable as the first object in our _IOleClientSiteEx
		// struct (which happens to be an IOleClientSite). So if the browser is asking us to match
		// IID_IUnknown, then we'll also return a pointer to our _IOleClientSiteEx.

		if (riid == IID_IUnknown || riid == IID_IOleClientSite ) 
			*ppvObject = this;

		// If the browser is asking us to match IID_IOleInPlaceSite, then it wants us to return a pointer to
		// our IOleInPlaceSite struct. Then the browser will use the VTable in that struct to call our
		// IOleInPlaceSite functions.  It will also pass this same pointer to all of our IOleInPlaceSite
		// functions (except for Site_QueryInterface, Site_AddRef, and Site_Release. Those will always get
		// the pointer to our _IOleClientSiteEx.
		//
		// Actually, we're going to lie to the browser. We're going to return our own _IOleInPlaceSiteEx
		// struct, and tell the browser that it's a IOleInPlaceSite struct. It's ok. The first thing in
		// our _IOleInPlaceSiteEx is an embedded IOleInPlaceSite, so the browser doesn't mind. We want the
		// browser to continue passing our _IOleInPlaceSiteEx pointer wherever it would normally pass a
		// IOleInPlaceSite pointer.
		//else if (!memcmp(&riid, &IID_IOleInPlaceSite, sizeof(GUID)))
		else if ( riid == IID_IOleInPlaceSite )
			*ppvObject = &inplace;

		// If the browser is asking us to match IID_IDocHostUIHandler, then it wants us to return a pointer to
		// our IDocHostUIHandler struct. Then the browser will use the VTable in that struct to call our
		// IDocHostUIHandler functions.  It will also pass this same pointer to all of our IDocHostUIHandler
		// functions (except for Site_QueryInterface, Site_AddRef, and Site_Release. Those will always get
		// the pointer to our _IOleClientSiteEx.
		//
		// Actually, we're going to lie to the browser. We're going to return our own _IDocHostUIHandlerEx
		// struct, and tell the browser that it's a IDocHostUIHandler struct. It's ok. The first thing in
		// our _IDocHostUIHandlerEx is an embedded IDocHostUIHandler, so the browser doesn't mind. We want the
		// browser to continue passing our _IDocHostUIHandlerEx pointer wherever it would normally pass a
		// IDocHostUIHandler pointer. My, we're really playing dirty tricks on the browser here. heheh.
		// else if (!memcmp(&riid, &IID_IDocHostUIHandler, sizeof(GUID)))
		else if ( riid == IID_IDocHostUIHandler )
			*ppvObject = &ui;

		// For other types of objects the browser wants, just report that we don't have any such objects.
		// NOTE: If you want to add additional functionality to your browser hosting, you may need to
		// provide some more objects here. You'll have to investigate what the browser is asking for
		// (ie, what REFIID it is passing).
		else
		{
			*ppvObject = 0;
			return(E_NOINTERFACE);
		}

		return(S_OK);
	}

	virtual ULONG STDMETHODCALLTYPE AddRef(void) {
		return (1);
	}

	virtual ULONG STDMETHODCALLTYPE Release(void)
	{
		return (1);
	};


	// IOleClientSite
	virtual HRESULT STDMETHODCALLTYPE SaveObject(void) 
	{
		NOTIMPLEMENTED;
	}

	virtual HRESULT STDMETHODCALLTYPE GetMoniker(
		/* [in] */ DWORD dwAssign,
		/* [in] */ DWORD dwWhichMoniker,
		/* [out] */ __RPC__deref_out_opt IMoniker **ppmk) 
	{
		NOTIMPLEMENTED;
	}

	virtual HRESULT STDMETHODCALLTYPE GetContainer(
		/* [out] */ __RPC__deref_out_opt IOleContainer **ppContainer)
	{
		// Tell the browser that we are a simple object and don't support a container
		*ppContainer = 0;

		return(E_NOINTERFACE);
	}

	virtual HRESULT STDMETHODCALLTYPE ShowObject(void) 
	{
		return(NOERROR);
	}

	virtual HRESULT STDMETHODCALLTYPE OnShowWindow(
		/* [in] */ BOOL fShow) 
	{
		NOTIMPLEMENTED;
	}

	virtual HRESULT STDMETHODCALLTYPE RequestNewObjectLayout(void) {
		NOTIMPLEMENTED;
	}
};

size_t offset_inplace()
{
	return offsetof(IOleClientSiteEx, inplace);
}

size_t offset_frame()
{
	return offsetof(IOleClientSiteEx, frame);
}

size_t offset_ui()
{
	return offsetof(IOleClientSiteEx, ui);
}

size_t offset_external()
{
	return offsetof(IOleClientSiteEx, ext);
}

size_t offset_browser()
{
	return offsetof(IOleClientSiteEx, browserObject);
}

size_t offset_window()
{
	return offsetof(IOleClientSiteEx, window);
}


/***************************** EmbedBrowserObject() **************************
* Puts the browser object inside our host window, and save a pointer to this
* window's browser object in the window's GWL_USERDATA field.
*
* hwnd =		Handle of our window into which we embed the browser object.
*
* RETURNS: 0 if success, or non-zero if an error.
*
* NOTE: We tell the browser object to occupy the entire client area of the
* window.
*
* NOTE: No HTML page will be displayed here. We can do that with a subsequent
* call to either DisplayHTMLPage() or DisplayHTMLStr(). This is merely once-only
* initialization for using the browser object. In a nutshell, what we do
* here is get a pointer to the browser object in our window's GWL_USERDATA
* so we can access that object's functions whenever we want, and we also pass
* pointers to our IOleClientSite, IOleInPlaceFrame, and IStorage structs so that
* the browser can call our functions in our struct's VTables.
*/

IOleObject	* EmbedBrowser(HWND hwnd)
{
	LPCLASSFACTORY		pClassFactory;
	IOleObject			*browserObject;
	IWebBrowser2		*webBrowser2;
	RECT				rect;
	IOleClientSiteEx	*iOleClientSiteEx;

	// initialize ole, only do it once
	static int oleinit = 0;
	if (oleinit == 0)
		oleinit = SUCCEEDED(OleInitialize(NULL));

	// Get a pointer to the browser object's IClassFactory object via CoGetClassObject()
	pClassFactory = NULL;
	if (!CoGetClassObject(CLSID_WebBrowser, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER, NULL, IID_IClassFactory, (void **)&pClassFactory) )
	{
		// Call the IClassFactory's CreateInstance() to create a browser object
		if (!pClassFactory->CreateInstance(0, IID_IOleObject, (void **)&browserObject))
		{
			// Free the IClassFactory. We need it only to create a browser object instance
			pClassFactory->Release();
			pClassFactory = NULL;

			// Create our IOleClientSite for the browser object
			iOleClientSiteEx = new IOleClientSiteEx();

			// Save our HWND so our IOleInPlaceFrame functions can retrieve it.
			iOleClientSiteEx->window = hwnd;

			// link browserObject to inplace, inplace.OnPosRectChange() need it
			iOleClientSiteEx->browserObject = browserObject;

			// Give the browser a pointer to my IOleClientSite object
			if (!browserObject->SetClientSite((IOleClientSite *)iOleClientSiteEx))
			{
				// We can now call the browser object's SetHostNames function. SetHostNames lets the browser object know our
				// application's name and the name of the document in which we're embedding the browser. (Since we have no
				// document name, we'll pass a 0 for the latter). When the browser object is opened for editing, it displays
				// these names in its titlebar.
				//	
				// Oh yeah, the L is because we need UNICODE strings. And BTW, the host and document names can be anything
				// you want.
				browserObject->SetHostNames(L"BROWSER", NULL);

				GetClientRect(hwnd, &rect);

				// Let browser object know that it is embedded in an OLE container.
				if (!OleSetContainedObject((IUnknown *)browserObject, TRUE) &&

					// Set the display area of our browser control the same as our window's size
					// and actually put the browser object into our window.
					!browserObject->DoVerb(OLEIVERB_SHOW, NULL, (IOleClientSite *)iOleClientSiteEx, 0, hwnd, &rect) &&

					// Ok, now things may seem to get even trickier, One of those function pointers in the browser's VTable is
					// to the QueryInterface() function. What does this function do? It lets us grab the base address of any
					// other object that may be embedded within the browser object. And this other object has its own VTable
					// containing pointers to more functions we can call for that object.
					//
					// We want to get the base address (ie, a pointer) to the IWebBrowser2 object embedded within the browser
					// object, so we can call some of the functions in the former's table. For example, one IWebBrowser2 function
					// we intend to call below will be Navigate2(). So we call the browser object's QueryInterface to get our
					// pointer to the IWebBrowser2 object.
					!browserObject->QueryInterface(IID_IWebBrowser2, (void**)&webBrowser2))
				{
					// Ok, now the pointer to our IWebBrowser2 object is in 'webBrowser2'.

					// Let's call several functions in the IWebBrowser2 object to position the browser display area
					// in our window. The functions we call are put_Left(), put_Top(), put_Width(), and put_Height().
					webBrowser2->put_Left(0);
					webBrowser2->put_Top(0);
					webBrowser2->put_Width(rect.right);
					webBrowser2->put_Height(rect.bottom);

					// We no longer need the IWebBrowser2 object (ie, we don't plan to call any more functions in it
					// right now, so we can release our hold on it). Note that we'll still maintain our hold on the
					// browser object until we're done with that object.
					webBrowser2->Release();

					// Success
					return (browserObject);
				}
			}
			browserObject->Release();
			delete iOleClientSiteEx;
		}

		if (pClassFactory)
			pClassFactory->Release();
	}
	return NULL;
}

void  UnEmbedBrowser(IOleObject	* browserObject)
{
	IOleClientSiteEx * iOleClientSiteEx;
	if (browserObject && !browserObject->GetClientSite((IOleClientSite **)&iOleClientSiteEx)) {
		// release ClientSite
		browserObject->SetClientSite((IOleClientSite *)NULL);
		// close browser objects
		browserObject->Close(OLECLOSE_NOSAVE);
		browserObject->Release();
		delete iOleClientSiteEx;
	}
}
