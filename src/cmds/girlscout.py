from playwright.sync_api import sync_playwright, TimeoutError
import os
import requests
import json
import base64
import time as t

def get_echo360_cookies(cookie_file: str = "cookies.json"):
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=False)  # visible — MFA needs a human
        context = browser.new_context()
        page = context.new_page()

        page.goto("https://echo360.net.au")

        try:
            page.wait_for_url("https://echo360.net.au/content", timeout=3_000_000)
        except TimeoutError:
            print("Timed out waiting for login — closing browser...")
            browser.close()
            return 7

        cookies = context.cookies()
        with open(cookie_file, "w") as f:
            json.dump(cookies, f, indent=4)

        browser.close()
        print(f"Saved {len(cookies)} cookies to {cookie_file}")

    return 0

def check_auth(cookie_file: str) -> bool:
    now = t.time()
    session = requests.Session()
    with open(cookie_file) as f:
        cookies = json.load(f)
    
    expiries = {}

    for c in cookies:
        session.cookies.set(c["name"], c["value"], domain=c.get("domain", ""), path=c.get("path", "/"))
        name = c["name"]
        if name == "ECHO_JWT":
            payload = json.loads(_b64url_decode(c["value"].split(".")[1]))
            expiries["ECHO_JWT"] = payload["exp"]
        elif name == "CloudFront-Policy":
            raw = _cloudfront_b64_decode(c["value"])
            policy = json.loads(raw)
            expiries["CloudFront-Policy"] = (
                    policy["Statement"][0]["Condition"]["DateLessThan"]["AWS:EpochTime"]
            )
    
    expired = {k:v for k,v in expiries.items() if v < now + 300}
    if expired:
        details = ", ".join(f"{k} (expired {int(now - v)}s ago)" for k, v in expired.items())
        print(f"Cookie(s) expired: {details}")
        return False 

    if expiries:
        soonest_label, soonest_time = min(expiries.items(), key=lambda kv: kv[1])
        remaining = int(soonest_time - now)
        print(f"Local check ok. Earliest expiry: {soonest_label} in {remaining}s "
              f"({remaining // 60} min).")

    resp = session.get("https://echo360.net.au/user/enrollments", allow_redirects=True)
    if resp.status_code != 200 or "login" in resp.url.lower():
        print("Server rejected session — cookies invalid despite local expiry check passing.")
        return False 
    
    return True

def _b64url_decode(s: str) -> bytes:
    s += "=" * (-len(s) % 4)
    return base64.urlsafe_b64decode(s)
   
def _cloudfront_b64_decode(s: str) -> bytes:
    s = s.replace("-", "+")
    s = s.replace("_", "=")
    s = s.replace("~", "/")
    return base64.b64decode(s)

def main():
    cookie_file = "cookies.json"

    if os.path.exists("cookies.json"):
        valid = check_auth("cookies.json")
    else:
        print("No cookies found")
        valid = False

    if not valid:
        print("Fetching fresh cookies...")
        get_echo360_cookies(cookie_file)
        valid = check_auth(cookie_file)
        if valid:
            print("Cookies validated")
            return 0
        else:
            print("Still invalid after fresh login. Did you login?")
            return 2

    return valid

if __name__ == "__main__":
    main()
