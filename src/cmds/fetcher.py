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
        self.session = self.load_session("cookies.json")

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
        
    def watch(self, section_id: str, lecture_number: str):
        lessons = self._get_syllabus(section_id)
        target = lessons[int(lecture_number) - 1]

        medias = target["lesson"]["medias"]
        media_id = medias[0]["id"]  # mediaId is shared across sources for one lesson

        institute = "60d4291f-70de-44d8-a332-d7c51983738d"
        path = f"https://content.echo360.net.au/0000.{institute}/{media_id}/1/"

        audio_url = path + "s0q1.mp4"
        self.download_mp4(audio_url, "audio.mp4")

        screen1_url = path + "s1q1.mp4"
        self.download_mp4(screen1_url, "s1.mp4")

        if len(medias) == 2:
            screen2_url = path + "s2q1.mp4"
            self.download_mp4(screen2_url, "s2.mp4")
        else:
            print(f"Lecture {lecture_number}: single-screen recording, no s2.")
    
    def download_mp4(self, url: str, output_path: str):
        resp = self.session.get(url, stream=True)
        resp.raise_for_status()
        with open(output_path, "wb") as f:
            for chunk in resp.iter_content(chunk_size=1024*1024):
                f.write(chunk)

#    def make_lecture(self, video_url, audio_url, output_path):
#        video_path = "video.mp4"
#       audio_path = "audio.mp4"
#
#        self.download_mp4(video_url, video_path)
#        self.download_mp4(audio_url, audio_path)
#        cmd = [
#            "ffmpeg",
#            "-i", video_path,
#            "-i", audio_path,
#            "-map", "0:v:0",
#            "-map", "1:a:0",
#            "-c", "copy",
#            "-bsf:a", "aac_adtstoasc",
#            output_path,
#        ]
#        subprocess.run(cmd, check=True)        

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

    def _section_id_for_course(self, course_code: str) -> str:
        with open("courses.json") as f:
            courses = json.load(f)
        matches = [c for c in courses if c["courseCode"] == course_code]
        if not matches:
            raise ValueError(f"No course found for code {course_code}")
        # if multiple offerings exist (retaken courses), you'll want to disambiguate —
        # e.g. most recent term, or raise if ambiguous
        return matches[0]["url"]  # you named sectionId "url" in load()

    def _get_syllabus(self, section_id: str) -> list:
        resp = self.session.get(f"https://echo360.net.au/section/{section_id}/syllabus")
        resp.raise_for_status()
        return resp.json()["data"]

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
    fetcher = Fetcher("STAT3004", "1")
    fetcher.watch("faa364b6-a253-44fe-acac-1d1a438bf11a", "1")
