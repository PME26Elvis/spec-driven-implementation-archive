#include "mdedit/core.h"
#include "mdedit/json.h"

#include <stdio.h>
#include <string.h>

static int passed=0;
static int failed=0;

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return false; } } while (0)
#define RUN(x) do { if (x()) { ++passed; printf("PASS %s\n",#x); } else { ++failed; } } while (0)

static bool test_wildcards(void) {
    CHECK(md_wildmatch("*.c","editor.c"));
    CHECK(md_wildmatch("build*","build-asan"));
    CHECK(md_wildmatch("tests/?est_*.c","tests/test_core.c"));
    CHECK(md_wildmatch("evidence/*/ui-*.png","evidence/screenshots/ui-source.png"));
    CHECK(!md_wildmatch("*.c","README.md"));
    CHECK(!md_wildmatch("Src/*","src/core.c")); /* Linux matching is case-sensitive. */
    return true;
}

static bool test_path_normalization(void) {
    char path[MD_PATH_MAX];
    CHECK(md_path_normalize_relative("docs/./guide/../README.md",path));
    CHECK(strcmp(path,"docs/README.md")==0);
    CHECK(md_path_normalize_relative("a//b///c",path));
    CHECK(strcmp(path,"a/b/c")==0);
    CHECK(!md_path_normalize_relative("../escape",path));
    CHECK(!md_path_normalize_relative("/absolute",path));
    CHECK(!md_path_normalize_relative("a/../../escape",path));
    return true;
}

static bool test_json_configuration_shapes(void) {
    static const char valid[]=
        "{\"include_extensions\":[\".c\",\".md\"],\"follow_directory_symlinks\":false," 
        "\"nested\":{\"unicode\":\"\\u7e41\\u9ad4\"}}";
    MdJsonError error; MdJson *root=md_json_parse(valid,sizeof(valid)-1U,&error);
    CHECK(root!=NULL&&root->type==MD_JSON_OBJECT);
    const MdJson *array=md_json_get(root,"include_extensions");
    CHECK(array!=NULL&&array->type==MD_JSON_ARRAY&&array->as.array.len==2U);
    bool follow=true; CHECK(md_json_bool(md_json_get(root,"follow_directory_symlinks"),&follow)&&!follow);
    md_json_free(root);
    static const char *invalid[]={"{","{\"a\":1,\"a\":2}","[1,]","{\"x\":tru}","\"\\uD800\""};
    for (size_t i=0U;i<MD_ARRAY_LEN(invalid);++i) {
        root=md_json_parse(invalid[i],strlen(invalid[i]),&error);
        CHECK(root==NULL);
    }
    return true;
}

static bool test_prng_golden(void) {
    static const uint64_t expected[]={
        UINT64_C(5180492295206395165),
        UINT64_C(12380297144915551517),
        UINT64_C(13389498078930870103),
        UINT64_C(5599127315341312413)
    };
    MdPrng prng; md_prng_seed(&prng,1U);
    for (size_t i=0U;i<MD_ARRAY_LEN(expected);++i) CHECK(md_prng_next(&prng)==expected[i]);
    return true;
}

static bool test_digest_and_hex(void) {
    uint8_t digest[32],roundtrip[32]; char hex[65];
    md_sha256("abc",3U,digest); md_hex_encode(digest,32U,hex);
    CHECK(strcmp(hex,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")==0);
    CHECK(md_hex_decode(hex,64U,roundtrip,sizeof(roundtrip))&&memcmp(digest,roundtrip,32U)==0);
    CHECK(!md_hex_decode("xy",2U,roundtrip,1U));
    return true;
}

int main(void) {
    RUN(test_wildcards);
    RUN(test_path_normalization);
    RUN(test_json_configuration_shapes);
    RUN(test_prng_golden);
    RUN(test_digest_and_hex);
    printf("TEST_SUMMARY total=%d passed=%d failed=%d skipped=0\n",passed+failed,passed,failed);
    return failed==0?0:1;
}
