#!/usr/bin/python

import requests;

if __name__ == '__main__':
    kiosk = requests.get('https://iceportal.de/api1/rs/page/zeitungskiosk')
    kiosk.raise_for_status()

    result = kiosk.json()
    items = kiosk.json()['teaserGroups'][0]['items']

    for item in items:
        page = item['navigation']['href']
        zeitung = requests.get(f'https://iceportal.de/api1/rs/page/{page}')
        zeitung.raise_for_status()

        mag = zeitung.json()['url']
        dl = requests.get('https://iceportal.de/' + mag)
        dl.raise_for_status()

        with open(item['title'] + '.pdf', 'wb') as f:
            f.write(dl.content)
