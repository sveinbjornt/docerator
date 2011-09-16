/*
 
 Docerator Mac OS X application -- GUI for docerator.py
 
 MIT License
 
 Copyright (C) 2010 Sveinbjorn Thordarson  
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 
 */

#import "DoceratorAppDelegate.h"

@implementation DoceratorAppDelegate

- (id)init
{
	if (self = [super init]) 
	{
		loadedIconPath = nil;
		labels = [[NSMutableArray alloc] initWithCapacity: 256];
		smallLabels = [[NSMutableArray alloc] initWithCapacity: 256];
	}	
	return self;
}
	 
-(void)dealloc
{
	if (loadedIconPath != nil)
		[loadedIconPath release];
	[labels release];
	[smallLabels release];
	[super dealloc];
}
			 
#pragma mark App delegate functions

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification 
{
	[window registerForDraggedTypes: [NSArray arrayWithObject: NSFilenamesPboardType]];
}

- (NSString *)iconPath
{
	return loadedIconPath;
}

#pragma mark Interface actions

- (IBAction)clear:(id)sender
{
	if (loadedIconPath != nil)
	{
		[loadedIconPath release];
		loadedIconPath = nil;
	}
	[appIconFilenameLabel setStringValue: @""];
	[createIconButton setEnabled: NO];
	[appIconImageView clear];
	
	[labels removeAllObjects];
	[smallLabels removeAllObjects];
	[iconLabelsTableView reloadData]; 
	
	[dropAppIconHereLabel setHidden: NO];
	
		[iconLabelsTableView setEnabled: YES];
	[size512Checkbox setEnabled: NO];
	[size256Checkbox setEnabled: NO];
	[size128Checkbox setEnabled: NO];
	[size32Checkbox setEnabled: NO];
	[size16Checkbox setEnabled: NO];
}

- (IBAction)createIcon:(id)sender
{
	//create open panel
	NSOpenPanel *oPanel = [NSOpenPanel openPanel];
	[oPanel setPrompt:@"Create"];
	[oPanel setAllowsMultipleSelection:NO];
	[oPanel setCanChooseDirectories: YES];
	[oPanel setCanChooseFiles: NO];
	
	//run open panel
	[oPanel beginSheetForDirectory:nil file:nil types:nil modalForWindow: window modalDelegate: self didEndSelector: @selector(createIconDidEnd:returnCode:contextInfo:) contextInfo: nil];
}

- (void)createIconDidEnd:(NSOpenPanel *)oPanel returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	if (returnCode != NSOKButton)
		return;
	
	[NSTimer scheduledTimerWithTimeInterval: 0.01 target: self selector:@selector(runDoceratorTool:) userInfo: [oPanel filename] repeats: NO];
}

- (void)runDoceratorTool: (NSTimer*)theTimer
{
	// first, get arguments from control settings
	NSMutableArray *args = [self generateDoceratorShellCommand: [theTimer userInfo]];
	if (args == nil)
		return;
	
	// show progress sheet
	[progressIndicator setUsesThreadedAnimation: YES];
	[progressIndicator startAnimation: self];
	
	[NSApp beginSheet: progressWindow
	   modalForWindow: window
		modalDelegate: nil
	   didEndSelector: nil
		  contextInfo: nil];
	
	// run shell task
	NSTask *doceratorTask = [[NSTask alloc] init];
	[doceratorTask setLaunchPath: @"/usr/bin/python"];
	[doceratorTask setCurrentDirectoryPath: [[NSBundle mainBundle] resourcePath]]; // we run from Resources directory which has helper tools
	[doceratorTask setArguments: args];
	[doceratorTask launch];
	[doceratorTask waitUntilExit];
	[doceratorTask release];
	[args release];
	
	// End progress sheet.
	[progressIndicator stopAnimation: self];
    [NSApp endSheet: progressWindow];
    [progressWindow orderOut: self];

}

- (NSMutableArray *)generateDoceratorShellCommand: (NSString *)destinationPath
{
	// create string specifying icon sizes
	NSMutableString *sizesStr = [NSMutableString stringWithCapacity: 100];
	if ([size512Checkbox intValue])
		[sizesStr appendString: @"512,"];
	if ([size256Checkbox intValue])
		[sizesStr appendString: @"256,"];
	if ([size128Checkbox intValue])
		[sizesStr appendString: @"128,"];
	if ([size32Checkbox intValue])
		[sizesStr appendString: @"32,"];
	if ([size16Checkbox intValue])
		[sizesStr appendString: @"16,"];
	if ([sizesStr length] > 0)
		[sizesStr deleteCharactersInRange: NSMakeRange([sizesStr length]-1, 1)];
	
	if ([sizesStr length] == 0)
	{
		[self sheetAlert: @"Invalid sizes" subText: @"You must select at least one size for the icon(s)."];
		return nil;
	}
	
	//sample cmd
	//python docerator.py --sizes 512,256,128,32,16 --appicon /path/to/icon.icns --text DDS:
	NSMutableArray *arguments = [[NSMutableArray alloc] initWithCapacity: 256];
	
	[arguments addObjectsFromArray: [NSArray arrayWithObjects:  
									 [[NSBundle mainBundle] pathForResource: @"docerator.py" ofType: nil],
									 @"--sizes", sizesStr,
									 @"--appicon", loadedIconPath,
									 @"--destination", destinationPath, nil ]];
	
	// add --text option for each label
	int i;
	for (i = 0; i < [labels count]; i++)
	{
		[arguments addObject: @"--text"];
		NSString *labelStr = [NSString stringWithString: [labels objectAtIndex: i]];
		if (![[smallLabels objectAtIndex: i] isEqualToString: @""])
			labelStr = [labelStr stringByAppendingFormat:@",%@", [smallLabels objectAtIndex: i]];
		[arguments addObject: labelStr];
	}
	
	return arguments;
}

- (IBAction)addLabel:(id)sender
{	
	[labels addObject: @"LABEL"];
	[smallLabels addObject: @""];
	
	[iconLabelsTableView reloadData];
}

- (IBAction)removeSelectedLabel: (id)sender
{
	[labels removeObjectAtIndex: [iconLabelsTableView selectedRow]];
	[smallLabels removeObjectAtIndex: [iconLabelsTableView selectedRow]];
	[iconLabelsTableView reloadData];
	[self tableViewSelectionDidChange: nil];
}


#pragma mark Opening and loading file

- (IBAction)openFile:(id)sender
{
		//create open panel
		NSOpenPanel *oPanel = [NSOpenPanel openPanel];
		[oPanel setPrompt:@"Select Icon"];
		[oPanel setAllowsMultipleSelection:NO];
		[oPanel setCanChooseDirectories: NO];
				
		//run open panel
	[oPanel beginSheetForDirectory:nil file:nil types: [NSArray arrayWithObjects: @"icns", @"app", nil] modalForWindow: window modalDelegate: self didEndSelector: @selector(openFileDidEnd:returnCode:contextInfo:) contextInfo: nil];
}

- (void)openFileDidEnd:(NSOpenPanel *)oPanel returnCode:(int)returnCode contextInfo:(void *)contextInfo
{	 
	[NSApp endSheet: window];
	[NSApp stopModal];
	if (returnCode != NSOKButton)
		return;
	
	[self loadFile: [oPanel filename]];
}

- (NSString *)icnsForAppBundle: (NSString *)appBundlePath
{
	// get Info.plist for app bundle
	NSBundle *appBundle = [NSBundle bundleWithPath: appBundlePath];
	NSDictionary *infoPlist = [appBundle infoDictionary];
	if (infoPlist == nil)
		return nil;
	
	// get path to the icon
	NSString *iconFileName = [infoPlist objectForKey: @"CFBundleIconFile"];
	if (iconFileName == nil || [iconFileName isEqualToString: @""])
		return nil;
	
	iconFileName = [iconFileName stringByDeletingPathExtension];
	NSString *icnsPath = [appBundle pathForResource: iconFileName ofType: @"icns"];
	
	return icnsPath;
}

- (BOOL)loadFile: (NSString *)filePath
{
	NSString *icnsPath;
	
	if ([filePath hasSuffix: @"icns"])
		icnsPath = filePath;
	else
	{
		icnsPath = [self icnsForAppBundle: filePath];
		if (icnsPath == nil || [icnsPath isEqualToString: @""])
			return NO;
	}
	return [self loadIcnsFile: icnsPath];
}

- (BOOL)loadIcnsFile: (NSString *)filePath
{
	if (![filePath hasSuffix: @"icns"])
		return NO;

	// load icon as image
	NSImage *img = [[[NSImage alloc] initByReferencingFile: filePath] autorelease];
	if (img == NULL)
		return NO;
	
	// store icon path
	loadedIconPath = [[NSString alloc] initWithString: filePath];
	
	// get the largest sized icon representation
	NSArray *reps = [img representations];
	int i, highestRep = 0;
	for (i = 0; i < [reps count]; i++)
	{
		int height = [[reps objectAtIndex: i] pixelsHigh];
		if (height > highestRep)
			highestRep = height;
	}
	
	// update controls to reflect that we've opened an icns file
	[size512Checkbox setEnabled: YES];
	[size256Checkbox setEnabled: YES];
	[size128Checkbox setEnabled: YES];
	[size32Checkbox setEnabled: YES];
	[size16Checkbox setEnabled: YES];
	
	[iconLabelsTableView setEnabled: YES];
	
	[size512Checkbox setIntValue: (highestRep >= 512)];
	[size256Checkbox setIntValue: (highestRep >= 256)];
	[size128Checkbox setIntValue: (highestRep >= 128)];
	[size32Checkbox setIntValue: (highestRep >= 32)];
	[size16Checkbox setIntValue: (highestRep >= 16)];
		
	[addLabelButton setEnabled: YES];
	
	[appIconImageView setImage: img];
	
	[appIconFilenameLabel setStringValue: [loadedIconPath lastPathComponent]];				  

	[dropAppIconHereLabel setHidden: YES];
	
	[createIconButton setEnabled: YES];
	
	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL: [NSURL fileURLWithPath: filePath]];
	return YES;
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	if (![filename hasSuffix: @"icns"] && ![filename hasSuffix: @"app"])
		return NO;

	return [self loadFile: filename];
}

#pragma mark Command string window

- (IBAction)showCommandString:(id)sender
{
	NSString *commandStr = @"/usr/bin/python ";
	NSMutableArray *args = [self generateDoceratorShellCommand: [@"~/Desktop/" stringByExpandingTildeInPath]];
	
	int i;
	for (i = 0; i < [args count]; i++)
	{
		commandStr = [commandStr stringByAppendingFormat: @"%@ ", [args objectAtIndex: i]];
	}
	
	[commandStringTextView setString: commandStr];

	[NSApp beginSheet: commandStringWindow
	   modalForWindow: window
		modalDelegate: self
	   didEndSelector: nil
		  contextInfo: nil];

	[NSApp runModalForWindow: window];
}

- (IBAction)closeCommandStringWindow: (id)sender
{
	[NSApp endSheet: commandStringWindow];
	[NSApp stopModal];
	[commandStringWindow orderOut: self];
}

#pragma mark Table view datasource

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
	return([labels count]);
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
	if ([[aTableColumn identifier] isEqualToString: @"1"])
		return [labels objectAtIndex: rowIndex];
	else if ([[aTableColumn identifier] isEqualToString: @"2"])
		return [smallLabels objectAtIndex: rowIndex];
	
	return @"";
}

- (void)tableView:(NSTableView *)aTableView setObjectValue: anObject forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
	if ([[aTableColumn identifier] isEqualToString: @"1"])
		[labels replaceObjectAtIndex: rowIndex withObject: anObject];
	else if ([[aTableColumn identifier] isEqualToString: @"2"])
		[smallLabels replaceObjectAtIndex: rowIndex withObject: anObject];
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
	int selected = [iconLabelsTableView selectedRow];

	if (selected != -1) //there is a selected item
		[removeLabelButton setEnabled: YES];
	else
		[removeLabelButton setEnabled: NO];
}

#pragma mark Menus

- (BOOL)validateMenuItem:(NSMenuItem*)anItem 
{
	if (([anItem action] == @selector(showCommandString:)) && [appIconImageView image] == nil)
		return NO;
	if (([anItem action] == @selector(createIcon:)) && [createIconButton isEnabled] == NO)
		return NO;
	
	return YES;
}

#pragma mark Drag and drop

/*****************************************
 - Dragging and dropping for window
 *****************************************/

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
	NSPasteboard	*pboard = [sender draggingPasteboard];
	
	if ([sender draggingSource] == appIconImageView)
		return NO;
	
    if ([[pboard types] containsObject: NSFilenamesPboardType]) 
	{
        NSArray *files = [pboard propertyListForType:NSFilenamesPboardType];
		NSString *filename = [files objectAtIndex: 0];//we only load the first dragged item
		if ([[NSFileManager defaultManager] fileExistsAtPath: filename])
		{
			if ([filename hasSuffix: @"icns"] || [filename hasSuffix: @"app"] )
				return [self loadFile: filename];
		}
	}
	
	return NO;
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender 
{
	if ([sender draggingSource] == appIconImageView)
		return NSDragOperationNone;
	
	// we accept dragged files
    if ([[[sender draggingPasteboard] types] containsObject:NSFilenamesPboardType])
	{
		NSArray *files = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
		int i;
		
		for (i = 0; i < [files count]; i++)
		{
			if ([[files objectAtIndex: i] hasSuffix: @"icns"] || [[files objectAtIndex: i] hasSuffix: @"app"])
				return NSDragOperationLink;
		}
    }
	return NSDragOperationNone;
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
	return [self draggingEntered: sender];
}

#pragma mark Alerts


- (void)sheetAlert: (NSString *)message subText: (NSString *)subtext
{
	NSAlert *alert = [[NSAlert alloc] init];
	[alert addButtonWithTitle:@"OK"];
	[alert setMessageText: message];
	[alert setInformativeText: subtext];
	[alert setAlertStyle: NSCriticalAlertStyle];
	
	[alert beginSheetModalForWindow: window modalDelegate:self didEndSelector: nil contextInfo:nil];
	[alert release];
}

#pragma mark - help

- (IBAction) openWebsite: (id)sender
{
	[[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString: @"http://code.google.com/p/docerator/"]];
}


@end
