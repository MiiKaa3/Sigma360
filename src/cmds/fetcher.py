import base64
import json 
import time as t
import requests

class Fetcher():

    def __init__(self, course_code: str, lecture_number: str):
        self.course_code = course_code
        self.lecture_number = lecture_number
        self.session = self.load_session("cookies.json")

    def load(self):
        if not self.check_auth():
            print("Failed authentication")
            return 1

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
        
    def watch(self, section_id: str, lecture_number: str, output_path:str):
        if not self.check_auth():
            print("Failed authentication")
            return 1

        lessons = self._get_syllabus(section_id)
        target = lessons[int(lecture_number) - 1]

        medias = target["lesson"]["medias"]
        media_id = medias[0]["id"]  # mediaId is shared across sources for one lesson

        institute = "60d4291f-70de-44d8-a332-d7c51983738d"
        path = f"https://content.echo360.net.au/0000.{institute}/{media_id}/1/"

        audio_url = path + "s0q1.mp4"
        self.download_mp4(audio_url, output_path + "audio.mp4")

        screen1_url = path + "s1q1.mp4"
        self.download_mp4(screen1_url, output_path + "s1.mp4")

        screen2_url = path + "s2q1.mp4"
        has_screen2 = self.download_mp4(screen2_url, output_path + "s2.mp4")
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
    
if __name__ == "__main__":
    fetcher = Fetcher("STAT3004", "2")
    fetcher.watch("faa364b6-a253-44fe-acac-1d1a438bf11a", "2", "../test/")
