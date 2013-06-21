
#import "aboutBoxMac.h"


#include <Cocoa/Cocoa.h>





@interface MyClass : NSObject

@property (strong) NSWindow * mwin;
@property (strong) NSMenu * theMenu;


- (void) openBox;
- (void) closeBox;
- (void) createMenu;

@end




__strong static MyClass * abox = 0;


@implementation MyClass

@synthesize mwin;
@synthesize theMenu;


- (void) closeBox
{
    
    [NSApp abortModal];
   
}//closebox

- (void) openBox
{
    NSRect frame = NSMakeRect(0, 0, 300, 300);
    mwin  = [[[NSWindow alloc] initWithContentRect:frame
                                        styleMask:NSTitledWindowMask
                                          backing:NSBackingStoreBuffered
                                            defer:NO] autorelease];
    
    
    NSButton * bn;
    bn = [[[NSButton alloc] initWithFrame:NSMakeRect(10, 10, 100, 20) ] autorelease];
    
    [bn setButtonType:NSMomentaryPushInButton];
    [bn setTitle:@"Ok"];
    [bn setTarget:self];
    [bn setAction:@selector(closeBox)];
    
    [[mwin contentView] addSubview:bn];


    NSBundle * splayerBundle = [NSBundle bundleWithIdentifier: @"com.FireBreath.SPlayer"];
    NSImage * image = [splayerBundle imageForResource: @"caelogo.png"]; //do not release or autorelease
    
    
    NSImageView * iview = [[[NSImageView alloc] initWithFrame:NSMakeRect(10,100,140,140)] autorelease] ;
    
    [iview setImage:image];
    
    [[mwin contentView] addSubview:iview];
    
    
    NSTextField * field;
    field = [[[NSTextField alloc] initWithFrame:NSMakeRect(160, 100, 100, 100)]autorelease];
    [field setSelectable:false];
    [field setEditable:false];
    [field setBezeled:false];
    [field setDrawsBackground:false];
    [field setStringValue:@"SPlayer by CAE 2013"];
    [[mwin contentView] addSubview:field];
    
    
    [NSApp runModalForWindow:mwin];
   
}//openbox


- (void) createMenu
{
    NSMenuItem * item;
    
    theMenu = [[[NSMenu alloc] initWithTitle:@"Contextual Menu"] autorelease];
    [theMenu insertItemWithTitle:@"Learningspace SPlayer" action:@selector(nope:) keyEquivalent:@"" atIndex:0];
    [theMenu insertItemWithTitle:@"---------------" action:@selector(nope:) keyEquivalent:@"" atIndex:1];
    item = [theMenu insertItemWithTitle:@"About" action:@selector(openBox)                   keyEquivalent:@"" atIndex:2 ];
    
    [item setTarget:self ];
    
    
    [theMenu popUpMenuPositioningItem:nil atLocation:[NSEvent mouseLocation] inView:nil];



}//createmenu

@end



void createMenu(void)
{
    
        abox = 0;
        abox = [[MyClass alloc] autorelease];
    
        [abox createMenu];
    
}//createmenu
