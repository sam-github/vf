/*
 * BeTar: a test driver for the Tar file reader classes (under BeOS).
 *
 * $Id$
 * $Log$
 * Revision 1.1  1999/04/11 01:56:54  sam
 * Initial revision
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

//#include <Application.h> 
//#include <ListView.h>
//#include <StringView.h>
//#include <Entry.h>
//#include <StringView.h>
//#include <Window.h>

#include "cctar.h"

class Alert
{
public:
	Alert(int level, const char* format, ...)
	{
		if(level >= threshold_) { return; }
		
		char message[BUFSIZ];
	
		va_list al;
		va_start(al, format);
		vsprintf(message, format, al);
		va_end(al);
		
		BAlert* alert
			= new BAlert("Tar",
				message,
				"Acknowledge", 0, 0,
				B_WIDTH_FROM_WIDEST);
		alert->Go();
	}
	
	void SetThreshold(int threshold)
	{
		threshold_ = threshold;
	}
private:
	static int threshold_;
};

int Alert::threshold_ = 2;

class TarView : public BListView
{
public:
	TarView(BRect rect) :
		BListView(rect, "Tar View", B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL, B_WILL_DRAW)
	{
	}
	~TarView()
	{
		// delete all items in list view
	}
};

class TarWin : public BWindow
{
public:
	TarWin(const char* path) :
			BWindow(BRect(100.0, 80.0, 560.0, 620.0),
				"Tar Window",
				B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, 0)
	{
		assert(path);
		TarView*     view = new TarView(Bounds());

		BStringItem* item = new BStringItem(path);
		view->AddItem(item);

		TarDir   tarDir;
		if(!tarDir.Open(path))
		{
			Alert(0, "tarDir.Open(%s) failed: [%d] %s",
				path, tarDir.ErrorNo(), tarDir.ErrorString());
			Quit();
		}
		Alert(1, "dir path %s", path);
		for(TarDir::iterator it = tarDir.begin(); it; ++it)
		{
			Alert(1, "it path %s", it.Path());
			item = new BStringItem(it.Path());
			view->AddItem(item);
		}

		AddChild(view);	
		Show();
	}
	
	virtual bool QuitRequested()
	{
		Alert(2, "QuitRequested()");
		be_app->PostMessage(B_QUIT_REQUESTED);
		return true;
	}
};

class TarApp : public BApplication
{
public:
	TarApp() :
		BApplication("application/x-vnd.morrigan-tar"),
		filePanel_(), // accept all defaults
		windows_(0)
			
	{
		Alert(2, "TarApp::TarApp()");
	/*
		BRect frame;
		frame.Set(100, 80, 260, 120);
		TarWin* window = new TarWin(frame);
	*/
	}
	
	void ArgvReceived(int32 argc, char* argv[])
	{
		Alert(2, "TarApp::ArgvReceived() argc %d argv[0] %s", argc, argv[0]);

		int opt;
		while((opt = getopt(argc, argv, "f:")) != -1)
		{
			switch(opt)
			{
			case 'f': OpenTar(optarg); break;
			default:
				break;
			}
		}
	}
		
	void ReadyToRun()
	{
		Alert(2, "TarApp::ReadyToRun()");
		
		if(windows_ == 0)
		{
			filePanel_.Show();
		}
	}

	void RefsReceived(BMessage *message) 
	{
		
		uint32 type; 
		int32 count; 
		entry_ref ref; 
		
		message->GetInfo("refs", &type, &count); 

		Alert(2, "TarApp::RefsReceived() type %#x count %d", type, count);
		assert(type == B_REF_TYPE);

		for ( long i = 0; i < count; i++ )
		{ 
			if(message->FindRef("refs", i, &ref) == B_OK)
			{
				BEntry entry;
				if(entry.SetTo(&ref, true) != B_NO_ERROR)
				{
					Alert(0, "entry.SetTo() failed");
					continue;
				}
				BPath path;
				if(entry.GetPath(&path))
				{
					Alert(0, "entry.GetPath() failed");
					continue;
				}
				Alert(1, path.Path());
 				OpenTar(path.Path());
			}
		}
	}

	bool QuitRequested()
	{
		Alert(2, "TarApp::QuitRequested(): windows %d", windows_);
		
		return (--windows_ <= 0) ? true : false;
	}

private:
	void OpenTar(const char* path)
	{
		TarWin* window = new TarWin(path);
		windows_++;
	}

	BFilePanel filePanel_;
	int windows_; // exit when all windows are closed
};

int main(void)
{	
	TarApp	app;
	Alert(2, "Begining");
	app.Run();
	Alert(2, "Ending");
	return B_NO_ERROR;
}
