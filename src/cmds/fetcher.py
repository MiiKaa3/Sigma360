import base64
import json 
import time as t
import requests
from bs4 import BeautifulSoup

def load_session(cookie_file:str) -> requests.Session:
    session = requests.Session()
    session.headers.update({
        "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
    })

    with open(cookie_file) as f:
        cookies = json.load(f)

    expiries = {}

    for c in cookies:
        session.cookies.set(
                c["name"], c["value"],
                domain=c.get("domain", "echo360.net.au"),
                path=c.get("path", "/"),
        )
        
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

    session.cookie_expiries = expiries
    return session

def _b64url_decode(s: str) -> bytes:
    s += "=" * (-len(s) % 4)
    return base64.urlsafe_b64decode(s)

def _cloudfront_b64_decode(s: str) -> bytes:
    s = s.replace("-", "+")
    s = s.replace("_", "=")
    s = s.replace("~", "/")
    return base64.b64decode(s)

def check_auth(session: requests.Session) -> bool:
    now = t.time()
    expiries = getattr(session, "cookie_expiries", {})

    expired = {k:v for k,v in expiries.items() if v < now}
    if expired:
        details = ", ".join(f"{k} (expired {int(now - v)}s ago)" for k, v in expired.items())
        print(f"Cookie(s) expired: {details}")
        return 67

    if expiries:
        soonest_label, soonest_time = min(expiries.items(), key=lambda kv: kv[1])
        remaining = int(soonest_time - now)
        print(f"Local check ok. Earliest expiry: {soonest_label} in {remaining}s "
              f"({remaining // 60} min).")

    resp = session.get("https://echo360.net.au/user/enrollments", allow_redirects=True)
    if resp.status_code != 200 or "login" in resp.url.lower():
        print("Server rejected session — cookies invalid despite local expiry check passing.")
        return 68
    
    return 0

if __name__ == "__main__":
    s = load_session("cookies.json")
    if check_auth(s):
        print("Auth accepted.")
        r = s.get("https://echo360.net.au/user/enrollments")
        print(r.status_code)
        print(r.url)
        soup = BeautifulSoup(r.text, "html.parser")
        print(soup.prettify())
    else:
        print("Auth failed. Cookies expired or missing.")


