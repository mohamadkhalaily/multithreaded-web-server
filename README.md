# Multithreaded Web Server

A simple web server implemented in C, extended to support multithreading for improved efficiency. This project is part of an academic operating systems course.

## Overview

The original provided web server was single-threaded, meaning it could handle only one client request at a time. This project enhances it by implementing multithreading, allowing concurrent handling of multiple client connections.

## Features

- Basic HTTP request parsing and response handling
- Multithreaded design using POSIX threads (`pthread`)
- Concurrent processing of multiple client connections
- Graceful handling of client disconnections
