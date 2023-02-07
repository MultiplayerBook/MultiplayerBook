Multiplayer Book
================


Welcome to the code repository for Multiplayer Game Programming: Architecting Networked Games.
Here you will find all the code samples to accompany the code from the book.

Supported Platforms
------------
The code has been tested and runs on Windows with Visual Studio 2019, using Win 32 - some chapters flag errors when using x64 target - 
and on Mac OS X with Xcode 7. It is planned to update this too, so watch this space...
Other platforms (including Linux) are currently not supported.

Installation
------------

To install, clone the repository to your local computer.

For the examples in Chapter 13, you'll need Node.js installed from https://nodejs.org
and you'll need to use a terminal or console window to switch to each of the subprojects in the Chapter 13 folder and type

```
npm install
```

That should grab all the necessary node modules and you'll be ready to go! However, some of the dependencies might be out of date.

```
npm audit
```
should flag any with security issues, and

```
npm audit fix --force
```
should allow the code to work. Sublime Text 3 project files have been verified, but it is also possible to use Visual Code, or 
to run the code from the command line with

```
node app.js
```

This runs the server code. To test it you will need 'curl' but this should already be installed for Windows 10, to create web requests, 
as per the chapter text.
