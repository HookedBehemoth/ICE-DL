fn main() {
    println!("cargo:rerun-if-changed=audio.c");
    cc::Build::new()
        .include("/usr/include/ffmpeg")
        .file("audio.c")
        .compile("audio");

        println!("cargo:rustc-link-lib=avformat");
        println!("cargo:rustc-link-lib=avcodec");
        println!("cargo:rustc-link-lib=avutil");
}