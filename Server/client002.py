#!/usr/bin/env python3

import requests

url = "https://54.253.68.173:4443/"

resp = requests.get(url, verify="cert.pem")
print(f"Status code: {resp.status_code}")
print(resp.text)