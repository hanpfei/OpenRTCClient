import json
import base64

# A 1x1 PNG image.
# Source: https://commons.wikimedia.org/wiki/File:1x1.png (Public Domain)
IMAGE = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAAACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII="

def main(request, response):
    response.headers.set(b'Access-Control-Allow-Origin', b'*')
    response.headers.set(b'Access-Control-Allow-Methods', b'OPTIONS, GET, POST')
    response.headers.set(b'Access-Control-Allow-Headers', b'Content-Type')

    response.headers.set(b"Cache-Control", b"max-age=3600");
    # CORS preflight
    if request.method == u'OPTIONS':
        return u''

    if b'some-etag' == request.headers.get(b"If-None-Match", None):
        response.status = 304
        return u''

    if request.GET.first(b"corp-cross-origin", default=b""):
        response.headers.set(b'Cross-Origin-Resource-Policy', b'cross-origin')

    response.headers.set(b'Etag', b'some-etag')
    response.headers.set(b'Content-Type', b'image/png')
    return base64.b64decode(IMAGE)
