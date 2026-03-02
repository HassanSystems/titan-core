import sys, urllib.request, urllib.parse, re
sys.stdout.reconfigure(encoding='utf-8')
try:
    url = 'https://html.duckduckgo.com/html/'
    data = urllib.parse.urlencode({'q': 'Iran Israel war'}).encode('utf-8')
    req = urllib.request.Request(url, data=data, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'})
    html = urllib.request.urlopen(req).read().decode('utf-8')
    results = re.findall(r'<a class="result__url" href="[^"]+">(.*?)</a>.*?<a class="result__snippet[^"]*"[^>]*>(.*?)</a>', html, re.IGNORECASE | re.DOTALL)
    for title, snippet in results[:3]:
        print(f"Title: {re.sub(r'<[^>]+>', '', title).strip()}\nBody: {re.sub(r'<[^>]+>', '', snippet).strip()}\n")
except Exception as e:
    print(f"Search error: {e}")
