/**
 * Copyright (c) 2000-2012 Liferay, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */

#import "ContentManager.h"
#import "IconCache.h"
#import "RequestManager.h"

#define READ_TAG 2422

static RequestManager* sharedInstance = nil;

@implementation RequestManager

- (id)init
{
	if ((self = [super init]))
	{
		_socket = [[GCDAsyncSocket alloc] initWithDelegate:self delegateQueue:dispatch_get_main_queue()];

		_isRunning = NO;
		_isConnected = NO;

		_registeredPathes = [[NSMutableDictionary alloc] init];

		[self start];
	}

	return self;
}

- (void)dealloc
{
	[_socket setDelegate:nil delegateQueue:NULL];
	[_socket disconnect];
	[_socket release];

	sharedInstance = nil;

	[super dealloc];
}

+ (RequestManager*)sharedInstance
{
	@synchronized(self)
	{
		if (sharedInstance == nil)
		{
			sharedInstance = [[self alloc] init];
		}
	}

	return sharedInstance;
}


- (void)askOnSocket:(NSString*)path query:(NSString*)verb
{
	NSString *query = [NSString stringWithFormat:@"%@:%@\n", verb,path];
	// NSLog(@"Query: %@", query);
	
	NSData* data = [query dataUsingEncoding:NSUTF8StringEncoding];
	[_socket writeData:data withTimeout:5 tag:4711];
	
	NSData* stop = [@"\n" dataUsingEncoding:NSUTF8StringEncoding];
	[_socket readDataToData:stop withTimeout:-1 tag:READ_TAG];
}


- (BOOL)isRegisteredPath:(NSString*)path
{
	// check if the file in question is underneath a registered directory
	NSArray *regPathes = [_registeredPathes allKeys];
	BOOL registered = NO;

	for( NSString *regPath in regPathes ) {
		if( [path hasPrefix:regPath]) {
			// the path was registered
			registered = YES;
			break;
		}
	}

	return registered;
}

- (NSNumber*)askForIcon:(NSString*)path isDirectory:(BOOL)isDir
{
	NSString *verb = @"RETRIEVE_FILE_STATUS";
	NSNumber *res = [NSNumber numberWithInt:0];

	if( [self isRegisteredPath:path] ) {
		if( _isConnected ) {
			if(isDir) {
				verb = @"RETRIEVE_FOLDER_STATUS";
			}

			[self askOnSocket:path query:verb];

			NSNumber *res_minus_one = [NSNumber numberWithInt:0];

			return res_minus_one;
		} else {
			[_requestQueue addObject:path];
			[self start]; // try again to connect
		}
	}
	return res;
}


- (void)socket:(GCDAsyncSocket*)socket didReadData:(NSData*)data withTag:(long)tag
{
	NSArray *chunks;
	NSString *answer = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
	if (answer != nil && [answer length] > 0) {
		// cut a trailing newline
		answer = [answer substringToIndex:[answer length] - 1];
		chunks = [answer componentsSeparatedByString: @":"];
	}
	NSLog(@"READ from socket (%ld): <%@>", tag, answer);
	ContentManager *contentman = [ContentManager sharedInstance];

	if( [chunks count] > 0 && tag == READ_TAG ) {
		if( [[chunks objectAtIndex:0] isEqualToString:@"STATUS"] ) {
			[contentman setResultForPath:[chunks objectAtIndex:2] result:[chunks objectAtIndex:1]];
		} else if( [[chunks objectAtIndex:0] isEqualToString:@"UPDATE_VIEW"] ) {
			NSString *path = [chunks objectAtIndex:1];
		} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"REGISTER_PATH"] ) {
			NSNumber *one = [NSNumber numberWithInt:1];
			NSString *path = [chunks objectAtIndex:1];
			[_registeredPathes setObject:one forKey:path];
			
			[contentman repaintAllWindows];
		} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"UNREGISTER_PATH"] ) {
			NSNumber *one = [NSNumber numberWithInt:1];
			NSString *path = [chunks objectAtIndex:1];
			[_registeredPathes removeObjectForKey:path];

			[contentman repaintAllWindows];
		} else {
			NSLog(@"Unknown command %@", [chunks objectAtIndex:0]);
		}
	} else {
		NSLog(@"Received unknown tag %ld", tag);
	}
	// Read on and on
	NSData* stop = [@"\n" dataUsingEncoding:NSUTF8StringEncoding];
	[_socket readDataToData:stop withTimeout:-1 tag:READ_TAG];

}

- (NSTimeInterval)socket:(GCDAsyncSocket*)socket shouldTimeoutReadWithTag:(long)tag elapsed:(NSTimeInterval)elapsed bytesDone:(NSUInteger)length
{
	// Called if a read operation has reached its timeout without completing.
	return 0.0;
}

- (void)socket:(GCDAsyncSocket*)socket didConnectToHost:(NSString*)host port:(UInt16)port
{
	NSLog( @"Connected to host successfully!");
	_isConnected = YES;
	_isRunning = NO;

	if( [_requestQueue count] > 0 ) {
		NSLog( @"We have to empty the queue");
		for( NSString *path in _requestQueue ) {
			[self askOnSocket:path];
		}
	}
	
	// Read for the UPDATE_VIEW requests
	NSData* stop = [@"\n" dataUsingEncoding:NSUTF8StringEncoding];
	[_socket readDataToData:stop withTimeout:-1 tag:READ_TAG];
	
}

- (void)socketDidDisconnect:(GCDAsyncSocket*)socket withError:(NSError*)err
{
	NSLog(@"Socket DISconnected!");

	_isConnected = NO;
	_isRunning = NO;
	if( err ) {
		NSLog(@"ERROR: %@", [err localizedDescription]);
	}

	// clear the registered pathes.
	[_registeredPathes release];
	_registeredPathes = [[NSMutableDictionary alloc] init];

    // clear the caches in conent manager
	ContentManager *contentman = [ContentManager sharedInstance];
	[contentman clearFileNameCacheForPath:nil];

	[NSTimer scheduledTimerWithTimeInterval:5 target:self selector:@selector(start) userInfo:nil repeats:NO];

}


- (void)start
{
	if (!_isRunning)
	{
		NSLog(@"Connect Socket!");
		NSError *err = nil;
		if (![_socket connectToHost:@"localhost" onPort:34001 withTimeout:5 error:&err]) // Asynchronous!
		{
			// If there was an error, it's likely something like "already connected" or "no delegate set"
			NSLog(@"I goofed: %@", err);
		}
		
		 _isRunning = YES;
	}
}

@end
