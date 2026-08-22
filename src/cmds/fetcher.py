import os
import base64
import json 
import time as t
from datetime import datetime
import requests
import argparse

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
COOKIE_FILE = os.path.join(SCRIPT_DIR, "..", "..", "cookies.json")

class Fetcher():

    def __init__(self):
                self.session = self.load_session("/src/cmds/cookies.json")
    
    # Functionality that retrieves and makes courses.json
    def load(self, outfile:str="../../courses.json"):
        if not self.check_auth():
            print("Failed authentication")
            return 1
        
        data = self.session.get("https://echo360.net.au/user/enrollments").json()
        loading_data = [] 
        sections = data["data"][0]["userSections"]
        activeSem = getYearSem((str(datetime.now())[:10]))
        for d in sections:
            cc = d["courseCode"]
            loading_data.append(
                                {"courseCode": cc,
                                "courseName": d["courseName"],
                                "url": d["sectionId"],
                                "lessonCount": d["lessonCount"],
                                "termId": d["termId"],
                                "yearSem": getYearSem(data["data"][0]["termsById"][d["termId"]]["startDate"]),
                                "isActive": getYearSem(data["data"][0]["termsById"][d["termId"]]["startDate"]) == activeSem
                                }
                                )
                             
        with open(outfile, "w") as f:
            json.dump(loading_data, f, indent=4)
        
        return 0
    
    # Functionality that retrieves the video files for a certain lecture
    def watch(self, output_path:str):
        if not self.check_auth():
            print("Failed authentication")
            return 1
        
        section_id, lecture_number = output_path.split("/")[-2:]
        lecture_number = lecture_number[7:]


        lessons = self._get_syllabus(section_id)
        target = lessons[int(lecture_number) - 1]

        medias = target["lesson"]["medias"]
        if len(medias) == 0:
            print("Lecture doesn't exist yet")
            return 2

        media_id = medias[0]["id"]  # mediaId is shared across sources for one lesson

        institute = "60d4291f-70de-44d8-a332-d7c51983738d"
        path = f"https://content.echo360.net.au/0000.{institute}/{media_id}/1/"

        audio_url = path + "s0q1.mp4"
        self.download_mp4(audio_url, output_path + "/audio.mp4")

        screen1_url = path + "s1q1.mp4"
        self.download_mp4(screen1_url, output_path + "/v1.mp4")

        screen2_url = path + "s2q1.mp4"
        has_screen2 = self.download_mp4(screen2_url, output_path + "/v2.mp4")
        if not has_screen2:
            print(f"Lecture {lecture_number}: single-screen recording, no s2.")

        return 0
    
    def download_mp4(self, url: str, output_path: str):
        resp = self.session.get(url, stream=True)
        if resp.status_code in (404, 403):
            return False
        resp.raise_for_status()
        with open(output_path, "wb") as f:
            for chunk in resp.iter_content(chunk_size=1024*1024):
                f.write(chunk)
        return True
    
   # Third Functionality is to load the thumbnails into the relevant directories. 
    def get_thumbnails(self, outpath: str):
        with open("courses.json") as f:
            all_courses = json.load(f)
        active_courses = [course for course in all_courses if course["isActive"]]
        
        institute = "60d4291f-70de-44d8-a332-d7c51983738d"

        for course in active_courses:
            lessons = self._get_syllabus(course["url"])
            for i, lesson in enumerate(lessons, start=1):
                for media in lesson["lesson"]["medias"]:
                    media_id = media["id"]
                    thumb_url = f"https://thumbnails.echo360.net.au/0000.{institute}/{media_id}/1/poster1.jpg"
                    code = course["url"]
                    path = f"{outpath}/{code}/Lecture{i}/t.jpg"
                    try:
                        self.download_mp4(thumb_url, path)
                    except:
                        print("Directory not made")      
                    t.sleep(0.2)
        
        return 0

    def load_session(self, cookie_file:str) -> requests.Session:
        session = requests.Session()
        session.headers.update({
            "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
        })
    
        with open(COOKIE_FILE) as f:
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
    
        resp = self.session.get("https://echo360.net.au/user/enrollments", allow_redirects=True)
        if resp.status_code != 200 or "login" in resp.url.lower():
            print("Server rejected session — cookies invalid despite local expiry check passing.")
            return False 
        
        return True

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

def main():
    parser = argparse.ArgumentParser(description="Sigma360 Echo360 fetcher")
    group = parser.add_mutually_exclusive_group(required=True)

    group.add_argument(
        "--load",
        nargs="?",
        const="../../courses.json",
        default=None,
        metavar="OUTFILE",
        help="Fetch and save the course list (default: courses.json)",
    )

    group.add_argument(
        "--watch",
        metavar="PATH",
        help="Download a specific lecture",
    )

    group.add_argument(
        "--thumbnails",
        metavar="OUTFILE",
        help="Get the thumbnails for each active course",
    )

    args = parser.parse_args()
    
    code = 0

    if args.load:
        outfile = args.load
        fetcher = Fetcher()
        print(f"Fetcher will load, saving to {outfile}")
        code = fetcher.load(outfile)

    elif args.watch:
        path = args.watch
        fetcher = Fetcher()
        print(f"Fetcher will fetch this lecture: {path}") 
        code = fetcher.watch(path)

    elif args.thumbnails:
        outpath = args.thumbnails
        fetcher = Fetcher()
        print(f"Fetcher will retrieve thumbnails for all active courses")
        code = fetcher.get_thumbnails(outpath)

    return code

if __name__ == "__main__":
    main()
