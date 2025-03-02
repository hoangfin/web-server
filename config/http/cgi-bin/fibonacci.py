import time

def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

print("Content-Type: text/html\r")
print("\r")

start_time = time.time()
result = fibonacci(5)  # Adjust the number for more/less processing time
end_time = time.time()

# HTML content with injected Python variables using f-strings
html_content = f"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CGI Example</title>
</head>
<body>
    <h1>Hello!</h1>
    <p>Computed Fibonacci(5) = {result} in {end_time - start_time:.2f} seconds</p>
</body>
</html>
"""

# Print the HTML content
print(html_content)
