#!/usr/bin/env python3
import os
import sys
import urllib.parse

def handle_get():
    """Handles GET requests by reading the query string."""
    query_string = os.environ.get("QUERY_STRING", "")
    params = urllib.parse.parse_qs(query_string)
    name = params.get("name", ["Guest"])[0]

    return f"<h1>GET Request Received</h1><p>Hello, {name}!</p>"

def handle_post():
    """Handles POST requests by reading the request body."""
    content_length = int(os.environ.get("CONTENT_LENGTH", 0))
    post_data = sys.stdin.read(content_length) if content_length > 0 else ""
    params = urllib.parse.parse_qs(post_data)
    name = params.get("name", ["Guest"])[0]

    return f"<h1>POST Request Received</h1><p>Hello, {name}!</p>"

def main():
    """Determines request type and processes it accordingly."""
    try:
        request_method = os.environ.get("REQUEST_METHOD", "GET")

        if request_method == "GET":
            response_body = handle_get()
        elif request_method == "POST":
            response_body = handle_post()
        else:
            response_body = "<h1>405 Method Not Allowed</h1>"
            print(response_body)
            return

        # Send HTTP response
        print(f"<html><body>{response_body}</body></html>")

    except Exception as e:
        # Log error and return 500 status
        sys.stderr.write(f"CGI Error: {str(e)}\n")
        print("<html><body><h1>Internal Server Error</h1></body></html>")

if __name__ == "__main__":
    main()
