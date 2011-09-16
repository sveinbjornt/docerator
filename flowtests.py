import functools
import glob
import os
import pprint
import re
import sys

def files():
  for line in open('flowtests.txt'):
    m = re.match(r'(.+) ([^ ]+) ([^ ]+)', line.strip())
    appname, docicon, icon = m.groups()
    rawname, = re.search(r'([^/]+).app$', appname).groups()
    yield appname, rawname, docicon, icon


def writehtml():
  js = \
"""
<script type="text/javascript"
  src="http://ajax.googleapis.com/ajax/libs/jquery/1.2.6/jquery.min.js">
</script>
<script type="text/javascript">
$(document).ready(function() {
  $('.rollover').hover(function() {
    var currentImg = $(this).attr('src');
    $(this).attr('src', $(this).attr('data-hover'));
    $(this).attr('data-hover', currentImg);
  }, function() {
    var currentImg = $(this).attr('src');
    $(this).attr('src', $(this).attr('data-hover'));
    $(this).attr('data-hover', currentImg);
  });
});
</script>
"""

  sizeDict = {}
  html = [js]
  for appname, rawname, docicon, icon in files():
    p = os.path.join('flowtests', rawname)

    leftImgs, rightImgs = [], []
    for index in xrange(0, 10):
      imgs = sorted(glob.glob(os.path.join(p, '%d_warped_*.png' % index)))
      if not imgs: continue
      leftImgs.append(imgs[-1])
      rightImgs.append(os.path.join(p, '%d_out.png' % index))

    if not leftImgs: continue

    # delete unused images
    imgs = set(glob.glob(os.path.join(p, '*.png')))
    for i in imgs.difference(rightImgs + leftImgs):
      os.remove(i)


    html.append('<table><tr>')

    html.append(
        '<td><img src="%s" data-hover="%s" class="rollover"></td>'
        % (leftImgs[0], rightImgs[0]))
    html.append('<td style="vertical-align:top">')
    for i in xrange(1, len(leftImgs)):
      html.append('<img src="%s" data-hover="%s"' % (leftImgs[i], rightImgs[i])
          + 'class="rollover" style="display:block">')
    html.append('</td>')

    html.append('<td style="vertical-align:top; direction:rtl">')
    for img in rightImgs[1:]:
      html.append('<img src="%s" style="display:block">' % img)
    html.append('</td>')
    html.append('<td><img src="%s"></td>' % rightImgs[0])

    html.append('</tr></table>')

    l = os.path.join(p, 'log.txt')

    keys = [16, 32, 128, 512, 256][0:len(leftImgs)]
    params = filter(functools.partial(re.match, r'(-?\d+(\.\d+)?){4}'), open(l))
    sizeDict[rawname] = dict(zip(sorted(keys, reverse=True),
      [tuple(map(float, s.strip().split())) for s in params]))

    html.append('<br>'.join(params))
    html.append('<a href="%s">Log</a><br style="clear:both">' % l)

  html.append('<pre>' + pprint.pformat(sizeDict) + '</pre>')
  open('flowtests.html', 'w').write('\n'.join(html))


if __name__ == '__main__':
  whitelist = sys.argv[1:]

  d = os.getcwd()
  for appname, rawname, docicon, icon in files():
    if whitelist and rawname not in whitelist: continue

    r = '/Applications/%s/Contents/Resources' % appname
    if not os.access(r, os.F_OK):
      print 'Skipping', r
      continue
    p = os.path.join(d, 'flowtests', rawname)

    os.chdir(d)
    writehtml()  # i'm impatient, first index what is there

    for f in glob.glob(os.path.join(p, '*.png')):
      os.remove(f)
    print p, docicon, icon
    if not os.access(p, os.F_OK):
      os.makedirs(p)
    os.chdir(p)
    os.system('../../flow "%s" "%s" | tee log.txt'
        % (os.path.join(r, docicon), os.path.join(r, icon)))

  os.chdir(d)
  writehtml()  # stuff might have changed
