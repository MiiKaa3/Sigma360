import base64
import json 
import time as t
import requests
from bs4 import BeautifulSoup

class Fetcher():

    def __init__(self, course_code: str, lecture_number: str):
        self.course_code = course_code
        self.lecture_number = lecture_number
        self.session = self.load_session("cook.json")

    def load(self):
        self.check_auth()

        data = self.session.get("https://echo360.net.au/user/enrollments").json()
        loading_data = [] 
        sections = data["data"][0]["userSections"]
        for d in sections:
            cc = d["courseCode"]
            loading_data.append(
                                {"courseCode": cc,
                                "courseName": d["courseName"],
                                "url": d["sectionId"],
                                "lessonCount": d["lessonCount"],
                                "termId": d["termId"],
                                "yearSem": getYearSem(data["data"][0]["termsById"][d["termId"]]["startDate"])
                                }
                                )
                             
        with open("courses.json", "w") as f:
            json.dump(loading_data, f, indent=4)
        
        return 0
        



    def fetch(self):
        self.check_auth()
        r = self.session.get("https://echo360.net.au/user/enrollments")
        data = r.json()
        print_tree(data)

    def load_session(self, cookie_file:str) -> requests.Session:
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
                payload = json.loads(self._b64url_decode(c["value"].split(".")[1]))
                expiries["ECHO_JWT"] = payload["exp"]
    
            elif name == "CloudFront-Policy":
                raw = self._cloudfront_b64_decode(c["value"])
                policy = json.loads(raw)
                expiries["CloudFront-Policy"] = (
                        policy["Statement"][0]["Condition"]["DateLessThan"]["AWS:EpochTime"]
                )
    
        session.cookie_expiries = expiries
        return session
    
    def _b64url_decode(self, s: str) -> bytes:
        s += "=" * (-len(s) % 4)
        return base64.urlsafe_b64decode(s)
    
    def _cloudfront_b64_decode(self, s: str) -> bytes:
        s = s.replace("-", "+")
        s = s.replace("_", "=")
        s = s.replace("~", "/")
        return base64.b64decode(s)
    
    def check_auth(self) -> bool:
        now = t.time()
        expiries = getattr(self.session, "cookie_expiries", {})
    
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
    
        resp = self.session.get("https://echo360.net.au/user/enrollments", allow_redirects=True)
        if resp.status_code != 200 or "login" in resp.url.lower():
            print("Server rejected session — cookies invalid despite local expiry check passing.")
            return 68
        
        return 0

def getYearSem(date: str) -> str:
        y, m, d = date.split("-")
        
        if m in ["12", "01", "02", "03"]:
            sem = "1"
        elif m == "11":
            sem = "3"
        else:
            sem = "2"

        if m == "12":
            year = str(int(y) + 1)
        else:
            year = y
        
        if sem == "3":
            return year + " Summer Semester"
        else:
            return year + " Semester " + sem
    
def print_tree(obj, indent=0):
    prefix = "    " * indent

    if isinstance(obj, dict):
        for key, value in obj.items():
            print(f"{prefix}{key}:")
            print_tree(value, indent + 1)

    elif isinstance(obj, list):
        for i, value in enumerate(obj):
            print(f"{prefix}[{i}]:")
            print_tree(value, indent + 1)

    else:
        print(f"{prefix}{obj}")

if __name__ == "__main__":
    fetcher = Fetcher("STAT3004", "12")
    fetcher.load()
    
