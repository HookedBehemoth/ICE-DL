// use reqwest;

use std::ffi::CString;

use reqwest::ClientBuilder;

#[repr(C)]
struct chapter {
    title: *const i8,
    url: *const i8,
}

#[repr(C)]
struct metadata {
    title: *const i8,
    thumbnail: *const i8,
    nb_chapters: u32,
    chapters: *const chapter,
}

struct ChapterOwning {
    title: CString,
    url: CString,
}

extern "C" {
    fn download(meta: *const metadata, output_filename: *const i8) -> i32;
}

#[derive(serde::Deserialize)]
struct Page {
    teaserGroups: Box<[TeaserGroup]>,
}

#[derive(serde::Deserialize)]
struct TeaserGroup {
    items: Box<[TeaserItem]>,
}

#[derive(serde::Deserialize)]
struct TeaserItem {
    title: CString,
    navigation: Navigation,
    picture: Picture,
    r#type: String,
}

#[derive(serde::Deserialize)]
struct Navigation {
    href: String,
}

#[derive(serde::Deserialize)]
struct Picture {
    src: String,
}

#[derive(serde::Deserialize)]
struct File {
    title: String,
    path: String,
}

#[derive(serde::Deserialize)]
struct Book {
    title: CString,
    picture: Picture,
    files: Box<[File]>,
}

const BASE_URL: &str = "https://iceportal.de";
const PAGE_URL: &str = "https://iceportal.de/api1/rs/page";
const AUDIO_URL: &str = "https://iceportal.de/api1/rs/audiobooks/path";

#[tokio::main]
async fn main() {
    use reqwest::header;
    let mut headers = header::HeaderMap::new();
    headers.insert("Cookie", header::HeaderValue::from_static("dbsession=10edb6d8.6077086c9abf5; s_fid=0EEE0C294B099A36-18947FC6829CC79E; gvo_v25=direct; s_cc=true; s_sq=%5B%5BB%5D%5D"));

    let client = ClientBuilder::new()
        .user_agent("Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/117.0")
        .default_headers(headers)
        .build()
        .unwrap();
    // println!("Hello, world!");
    let page_url = format!("{PAGE_URL}/hoerbuecher");
    let page = client.get(page_url).send().await.unwrap();
    if !page.status().is_success() {
        for header in page.headers() {
            eprintln!("{}: {}", header.0.as_str(), header.1.to_str().unwrap());
        }
        panic!("{:?}: {}", page.status(), page.text().await.unwrap());
    }
    let page = page.json::<Page>().await.unwrap();
    for item in page.teaserGroups[0].items.iter() {
        println!("Downloading: {}", item.title.to_str().unwrap());
        let book_url = format!("{PAGE_URL}/{}", item.navigation.href);
        let book = client.get(book_url).send().await.unwrap();
        if !book.status().is_success() {
            panic!("{:?}", book.status());
        }
        // panic!("{:?}" , book.text().await.unwrap());
        let book = book.json::<Book>().await.unwrap();
        dl_book(&client, &book).await;
    }
}
#[derive(serde::Deserialize)]
struct Joke {
    path: String,
}

async fn dl_book(client: &reqwest::Client, book: &Book) {
    let mut chapters_owning = Vec::with_capacity(book.files.len());
    for chapter in book.files.iter() {
        let actual_url = format!("{AUDIO_URL}{}", &chapter.path);
        println!("resolved: {actual_url}");
        let actual = client.get(actual_url).send().await.unwrap();
        if !actual.status().is_success() {
            panic!("{:?}", actual.status());
        }
        let actual = actual.json::<Joke>().await.unwrap();
        let actual = format!("{BASE_URL}{}", actual.path);
        println!("resolved: {actual}");
        chapters_owning.push(ChapterOwning {
            title: CString::new(chapter.title.as_str()).unwrap(),
            url: CString::new(actual).unwrap(),
        });
    }
    let chapters_owning = chapters_owning;
    let chapters: Vec<chapter> = chapters_owning
        .iter()
        .map(|c| chapter {
            title: c.title.as_ptr(),
            url: c.url.as_ptr(),
        })
        .collect();
    let thumbnail = CString::new(format!("{BASE_URL}/{}", book.picture.src)).unwrap();
    let metadata = metadata {
        title: book.title.as_ptr(),
        thumbnail: thumbnail.as_ptr(),
        nb_chapters: chapters.len() as _,
        chapters: chapters.as_ptr() as _,
    };

    let output = CString::new(format!("{}.m4a", book.title.to_str().unwrap())).unwrap();
    let ret = unsafe { download(&metadata as _, output.as_ptr()) };
    if ret != 0 {
        panic!("error while downloading {ret}");
    }
}
