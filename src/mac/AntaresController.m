// Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont
// Copyright (C) 2008-2012 The Antares Authors
//
// This file is part of Antares, a tactical space combat game.
//
// Antares is free software: you can redistribute it and/or modify it
// under the terms of the Lesser GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Antares is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with Antares.  If not, see http://www.gnu.org/licenses/

#include "mac/AntaresController.h"

#include <stdlib.h>
#include <ApplicationServices/ApplicationServices.h>
#include "mac/AntaresExtractDataController.h"
#include "mac/c/AntaresController.h"
#include "mac/c/scenario-list.h"

#define kScreenWidth @"ScreenWidth"
#define kScreenHeight @"ScreenHeight"
#define kScenario @"Scenario"
#define kSkipSettings @"SkipSettings"

#define kIdentifier @"Identifier"
#define kTitle @"Title"
#define kDownloadURL @"DownloadURL"
#define kAuthor @"Author"
#define kAuthorURL @"AuthorURL"
#define kVersion @"Version"

#define kFactoryScenario @"com.biggerplanet.ares"

// A comparator for display mode dictionaries.  Orders the objects as
// the pair (screen height, screen width).  The case where this ordering
// differs from (screen width, screen height) is unlikely enough that
// it's not worth doing anything fancier.
static NSInteger compare_resolutions(id x, id y, void* unused) {
    (void)unused;
    int x_width = CGDisplayModeGetWidth((CGDisplayModeRef)x);
    int x_height = CGDisplayModeGetHeight((CGDisplayModeRef)x);
    int y_width = CGDisplayModeGetWidth((CGDisplayModeRef)y);
    int y_height = CGDisplayModeGetHeight((CGDisplayModeRef)y);

    if (x_height < y_height) {
        return NSOrderedAscending;
    } else if (x_height > y_height) {
        return NSOrderedDescending;
    } else if (x_width < y_width) {
        return NSOrderedAscending;
    } else if (x_width > y_width) {
        return NSOrderedDescending;
    } else {
        return NSOrderedSame;
    }
}

static NSInteger compare_scenarios(id x, id y, void* unused) {
    (void)unused;

    return [[x objectForKey:kTitle] caseInsensitiveCompare:[y objectForKey:kTitle]];
}

static NSString* str(const char* utf8_bytes) {
    return [NSString stringWithUTF8String:utf8_bytes];
}

static NSURL* url(const char* utf8_bytes) {
    return [NSURL URLWithString:str(utf8_bytes)];
}

@interface AntaresController (Private)
- (NSURL*)authorURL;
- (void)setAuthorURL:(NSURL*)authorURL;
- (NSURL*)downloadURL;
- (void)setDownloadURL:(NSURL*)downloadURL;
@end

@implementation AntaresController
- (void)application:(NSApplication*)app openFile:(NSString*)filename {
    [_window orderOut:self];
    AntaresExtractDataController* extract = [[[AntaresExtractDataController alloc]
        initWithTarget:self selector:@selector(installDone:) path:filename] autorelease];
    if (!extract) {
        NSLog(@"Failed to create AntaresExtractDataController");
        exit(1);
    }
}

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
    bool skip =
        [[NSUserDefaults standardUserDefaults] boolForKey:kSkipSettings]
        && !([NSEvent modifierFlags]
                & NSDeviceIndependentModifierFlagsMask
                & NSAlternateKeyMask);
    if (skip) {
        [self settingsDone:self];
    } else {
        [_window center];
        [_window makeKeyAndOrderFront:self];
    }
}

- (void)setScenarioFrom:(NSMenuItem*)sender {
    NSString* identifier = [[sender representedObject] objectForKey:kIdentifier];
    NSString* title = [[sender representedObject] objectForKey:kTitle];
    NSString* author = [[sender representedObject] objectForKey:kAuthor];
    NSString* version = [[sender representedObject] objectForKey:kVersion];

    [_scenario_button setTitle:title];
    [self setDownloadURL:[[sender representedObject] objectForKey:kDownloadURL]];
    [_author_button setTitle:author];
    [self setAuthorURL:[[sender representedObject] objectForKey:kAuthorURL]];
    [_version_label setStringValue:version];

    [[NSUserDefaults standardUserDefaults] setObject:identifier forKey:kScenario];
}

- (void)updateScenarioList {
    NSString* user_scenario = [[NSUserDefaults standardUserDefaults] stringForKey:kScenario];
    int factory_scenario = -1;
    int best_scenario = -1;

    NSMutableArray* scenarios = [NSMutableArray array];
    AntaresScenarioList* list = antares_scenario_list_create();
    size_t i;
    for (i = 0; i < antares_scenario_list_size(list); ++i) {
        AntaresScenarioListEntry* entry = antares_scenario_list_at(list, i);
        NSString* title = str(antares_scenario_list_entry_title(entry));
        NSString* identifier = str(antares_scenario_list_entry_identifier(entry));
        NSDictionary* scenario_info = [NSDictionary dictionaryWithObjectsAndKeys:
            identifier, kIdentifier,
            title, kTitle,
            url(antares_scenario_list_entry_download_url(entry)), kDownloadURL,
            str(antares_scenario_list_entry_author(entry)), kAuthor,
            url(antares_scenario_list_entry_author_url(entry)), kAuthorURL,
            str(antares_scenario_list_entry_version(entry)), kVersion,
            NULL
        ];
        [scenarios addObject:scenario_info];
    }
    antares_scenario_list_destroy(list);
    [scenarios sortUsingFunction:compare_scenarios context:nil];

    [_scenario_list removeAllItems];
    for (i = 0; i < [scenarios count]; ++i) {
        NSDictionary* scenario = [scenarios objectAtIndex:i];
        NSString* title = [scenario objectForKey:kTitle];
        NSString* identifier = [scenario objectForKey:kIdentifier];

        NSMenuItem* menu_item = [[[NSMenuItem alloc] initWithTitle:title action:nil
            keyEquivalent:@""] autorelease];
        [menu_item setRepresentedObject:scenario];
        [menu_item setTarget:self];
        [menu_item setAction:@selector(setScenarioFrom:)];
        [[_scenario_list menu] addItem:menu_item];

        if ([identifier isEqualToString:user_scenario]) {
            best_scenario = i;
        }
        if ([identifier isEqualToString:kFactoryScenario]) {
            factory_scenario = i;
        }
    }

    if (best_scenario < 0) {
        best_scenario = factory_scenario;
    }

    [_scenario_list selectItemAtIndex:best_scenario];
    [self setScenarioFrom:[_scenario_list itemAtIndex:best_scenario]];
}

- (void)awakeFromNib {
    [self updateScenarioList];
    bool skip_settings = [[NSUserDefaults standardUserDefaults] boolForKey:kSkipSettings];
    [_skip_checkbox setIntValue:skip_settings];
}

- (IBAction)openScenarioURL:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL:_download_url];
}

- (IBAction)openAuthorURL:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL:_author_url];
}

- (IBAction)setSkipSettingsFrom:(id)sender {
    bool skip_settings = [sender intValue];
    [[NSUserDefaults standardUserDefaults] setBool:skip_settings forKey:kSkipSettings];
}

- (IBAction)settingsDone:(id)sender {
    [_window close];

    CFStringRef error_message;
    drivers = antares_controller_create_drivers(&error_message);
    if (!drivers) {
        NSLog(@"%@", error_message);
        CFRelease(error_message);
        exit(1);
    }

    NSString* scenario = [[NSUserDefaults standardUserDefaults] stringForKey:kScenario];
    AntaresExtractDataController* extract = [[[AntaresExtractDataController alloc]
        initWithTarget:self selector:@selector(extractDone:) scenario:scenario] autorelease];
    if (!extract) {
        NSLog(@"Failed to create AntaresExtractDataController");
        exit(1);
    }
}

- (void)installDone:(id)sender {
    [self updateScenarioList];
    [_window center];
    [_window makeKeyAndOrderFront:self];
}

- (void)extractDone:(id)sender {
    CFStringRef error_message;
    if (!antares_controller_loop(drivers, &error_message)) {
        NSLog(@"%@", error_message);
        CFRelease(error_message);
        exit(1);
    }
    antares_controller_destroy_drivers(drivers);
    [NSApp terminate:self];
}

- (void)dealloc {
    [_download_url release];
    [_author_url release];
    [super dealloc];
}

- (NSURL*)authorURL {
    @synchronized (self) {
        return [[_author_url retain] autorelease];
    }
}

- (void)setAuthorURL:(NSURL*)authorURL {
    @synchronized (self) {
        [authorURL retain];
        [_author_url release];
        _author_url = authorURL;
    }
}

- (NSURL*)downloadURL {
    @synchronized (self) {
        return [[_download_url retain] autorelease];
    }
}

- (void)setDownloadURL:(NSURL*)downloadURL {
    @synchronized (self) {
        [downloadURL retain];
        [_download_url release];
        _download_url = downloadURL;
    }
}
@end
