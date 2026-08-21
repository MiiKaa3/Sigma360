import base64
import json 
import time as t
import requests
from bs4 import BeautifulSoup
import subprocess

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
        
    def watch(self, url, path):
        self.check_auth()

        download_lesson(url, path)
        
    def download_lesson(self, manifest_url, output_path):
        cookies = cookie_header(self.session)
        cmd = [
                "ffmpeg",
                "-headers", f"Cookie: {cookies}\r\n",
                "-i", manifest_url,
                "-c", "copy",
                output_path,
        ]
        subprocess.run(cmd, check=True)        

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

def cookie_header(session: requests.Session, domain_filter: str = "echo360.net.au", name_filter:str = "") -> str:
    """Build a raw Cookie header string from a requests session."""
    pairs = [
        f"{c.name}={c.value}"
        for c in session.cookies
        if ((domain_filter in c.domain) or (name_filter in name))
    ]
    return "; ".join(pairs)



if __name__ == "__main__":
    fetcher = Fetcher("STAT3004", "12")
    fetcher.watch("https://content.echo360.net.au/0000.60d4291f-70de-44d8-a332-d7c51983738d/7bee4707-6a80-4cd1-b2fe-b88519a4ff15/1/s2_av.m3u8?x-uid=0166278b-7f37-4d8a-8672-a0c464c95943&x-instid=60d4291f-70de-44d8-a332-d7c51983738d&x-lid=G_8d86e9f9-f41c-4168-91ac-5254f1578d0e_faa364b6-a253-44fe-acac-1d1a438bf11a_2026-07-27T12%3A00%3A00.000_2026-07-27T13%3A00%3A00.000&x-sid=faa364b6-a253-44fe-acac-1d1a438bf11a&x-mid=7bee4707-6a80-4cd1-b2fe-b88519a4ff15&x-act=videoView&x-src=desktop", "output.mp4")
    #url = f"https://echo360.net.au/section/faa364b6-a253-44fe-acac-1d1a438bf11a/home"
    #r = fetcher.session.get(url)

    #print(r.status_code)
    #print(r.url)
    #print(r.headers.get("content-type"))
    #print(r.text[:2000])
