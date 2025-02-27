import time

def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

start_time = time.time()
result = fibonacci(40)  # Adjust the number for more/less processing time
end_time = time.time()

print(f"Computed Fibonacci(40) = {result} in {end_time - start_time:.2f} seconds")
