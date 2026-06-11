import os

import requests

SERVER_URL = os.getenv("SERVER_URL", "10.99.1.11")


def main():
    assert requests.get(SERVER_URL + "/ping").text == "Hello, World!"
    assert requests.post(SERVER_URL + "/ping").text == "Hello, World!"


if __name__ == "__main__":
    main()
