#!/usr/bin/python

import requests
import os

headers={
    'Cookie': 'dbsession=eefcd8be.63c3aa1585ff8; s_fid=676D89B8CD544D55-1AA6C04FAE737188; gpv_pn=zeitungskiosk; gpv_ln=S%C3%BCddeutsche%20Zeitung; gvo_v25=direct; s_cc=true; s_sq=%5B%5BB%5D%5D"'
}

if __name__ == '__main__':
    kiosk = requests.get('https://iceportal.de/api1/rs/page/zeitungskiosk', headers=headers)
    kiosk.raise_for_status()

    result = kiosk.json()
    items = kiosk.json()['teaserGroups'][0]['items']

    for item in items:
        path = 'kiosk/' + item['title'].replace('/', '_') + '.pdf'

        if os.path.exists(path):
            print('skipping')
            continue

        page = item['navigation']['href']
        zeitung = requests.get(f'https://iceportal.de/api1/rs/page{page}', headers=headers)
        zeitung.raise_for_status()

        print('downloading', item['title'])

        mag = zeitung.json()['navigation']['href']
        dl = requests.get('https://iceportal.de/' + mag)
        dl.raise_for_status()

        with open(path, 'wb') as f:
            f.write(dl.content)
