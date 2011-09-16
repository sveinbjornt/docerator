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

#import "DoceratorImageView.h"


@implementation DoceratorImageView

#pragma mark Delegate

- (void)setDelegate: (id)theDelegate
{
	delegate = theDelegate;
}

- (id)delegate
{
	return delegate;
}

#pragma mark Dragging

// make image view accept dropped file via delegate

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	if (delegate && [delegate respondsToSelector:@selector(draggingEntered:)]) 
		return [delegate draggingEntered:sender];
	else
		return [super draggingEntered:sender];
}

- (void)draggingExited:(id <NSDraggingInfo>)sender;
{
	if (delegate && [delegate respondsToSelector:@selector(draggingExited:)]) 
		return [delegate draggingExited:sender];
	else
		return [super draggingExited:sender];
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
	if (delegate && [delegate respondsToSelector:@selector(draggingUpdated:)]) 
		return [delegate draggingUpdated:sender];
	else
		return [super draggingUpdated:sender];
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
	if (delegate && [delegate respondsToSelector:@selector(prepareForDragOperation:)]) 
		return [delegate prepareForDragOperation:sender];
	else
		return [super prepareForDragOperation:sender];
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
	if (delegate && [delegate respondsToSelector:@selector(performDragOperation:)])
		return [delegate performDragOperation:sender];
	else
		return [super performDragOperation:sender];
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
	if (delegate && [delegate respondsToSelector:@selector(concludeDragOperation:)])
		[delegate concludeDragOperation:sender];
	else
		[super concludeDragOperation:sender];
}

#pragma mark - Drag source

- (void)mouseDown: (NSEvent*)event
{
	if ([self image] == nil)
		return;
	
    //get the Pasteboard used for drag and drop operations
    NSPasteboard *dragPasteboard = [NSPasteboard pasteboardWithName: NSDragPboard];
	
    //create a new image for our semi-transparent drag image
    NSImage *dragImage = [[NSImage alloc] initWithSize: [[self image] size]];     
    if (dragImage == nil)
		return;
	
	//OK, let's set the path to the icon file as pasteboard data
	[dragPasteboard declareTypes: [NSArray arrayWithObject: NSFilenamesPboardType] owner: self];
	[dragPasteboard setPropertyList: [NSArray arrayWithObject: [delegate iconPath]] forType: NSFilenamesPboardType];
		
    //draw our original image as 50% transparent
    [dragImage lockFocus];	
    [[self image] dissolveToPoint: NSZeroPoint fraction: .5];
    [dragImage unlockFocus];//finished drawing
    [dragImage setScalesWhenResized: YES];//we want the image to resize
	
    //execute the drag
	
	// get click co-ordinate and center image on click location if size less than 256
	NSPoint origin = [self bounds].origin;
	if ([[self image] size].width < 256)
	{
		origin = [self convertPoint:[event locationInWindow] fromView:nil];
		origin.x -= ([self image].size.width/2);
		origin.y -= ([self image].size.height/2);
	}
	
    [self dragImage: dragImage					//image to be displayed under the mouse
				 at: origin						//point to start drawing drag image
			 offset: NSZeroSize					//no offset, drag starts at mousedown location
			  event: event						//mousedown event
		 pasteboard: dragPasteboard				//pasteboard to pass to receiver
			 source: self						//object where the image is coming from
		  slideBack: YES];						//if the drag fails slide the icon back
    
	[dragImage release];
}

- (NSDragOperation)draggingSourceOperationMaskForLocal: (BOOL)flag
{
	if (flag)
		return NSDragOperationNone;
	
	return NSDragOperationCopy;
}

- (BOOL)ignoreModifierKeysWhileDragging
{
	return YES;
}

- (BOOL)acceptsFirstMouse: (NSEvent *)event 
{
	//so source doesn't have to be the active window
    return YES;
}

#pragma mark Image control

-(void)setImage: (NSImage *)newImage
{
	// get best representation
	NSArray *reps = [newImage representations];
	int i, highestRep = 0;
	for (i = 0; i < [reps count]; i++)
	{
		int height = [[reps objectAtIndex: i] pixelsHigh];
		if (height > highestRep)
			highestRep = height;
	}

	// make sure never larger than 256
	if (highestRep > 256)
		highestRep = 256;
	
	[newImage setScalesWhenResized: YES];
	[newImage setSize: NSMakeSize(highestRep,highestRep)];
	[super setImage: newImage];
}

-(void)clear
{
	[super setImage: nil];
}


@end
