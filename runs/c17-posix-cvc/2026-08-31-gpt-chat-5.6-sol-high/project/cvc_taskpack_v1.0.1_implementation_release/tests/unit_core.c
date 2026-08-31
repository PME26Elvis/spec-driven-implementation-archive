#include "cvc_common.h"
#include "cvc_sha256.h"
#include "cvc_json.h"
#include "cvc_glob.h"
#include "cvc_diff.h"
#include "cvc_object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int total=0,failed=0;
#define T(name,expr) do{total++;if(expr)printf("PASS %02d %s\n",total,name);else{printf("FAIL %02d %s\n",total,name);failed++;}}while(0)
static int hex_is(const uint8_t d[32],const char*h){char x[65];cvc_hex_encode(d,32,x);return !strcmp(x,h);}
static int jp(const unsigned char*p,size_t n){CvcJson*j=NULL;char*e=NULL;int r=cvc_json_parse(p,n,&j,&e);cvc_json_free(j);free(e);return r==0;}
static int js(const char*s){return jp((const unsigned char*)s,strlen(s));}
static int three(const char*b,const char*o,const char*t,const char*want,int wantc){CvcBuf r;int c=-1;if(cvc_three_way_text((const unsigned char*)b,strlen(b),(const unsigned char*)o,strlen(o),(const unsigned char*)t,strlen(t),&r,&c)<0)return 0;int ok=c==wantc&&(c|| (r.len==strlen(want)&&!memcmp(r.data,want,r.len)));cvc_buf_free(&r);return ok;}
static int myers_reconstruct(const char*a,const char*b){CvcLines A,B;CvcDiffOps op;cvc_lines_split((const unsigned char*)a,strlen(a),&A);cvc_lines_split((const unsigned char*)b,strlen(b),&B);if(cvc_myers_diff(&A,&B,&op)<0)return 0;CvcBuf r;cvc_buf_init(&r);size_t ai=0,bi=0;for(size_t k=0;k<op.n;k++){while(ai<op.v[k].ai&&bi<op.v[k].bi){cvc_buf_append(&r,A.v[ai].p,A.v[ai].n);ai++;bi++;}if(op.v[k].op=='-')ai++;else{cvc_buf_append(&r,B.v[bi].p,B.v[bi].n);bi++;}}while(bi<B.n){cvc_buf_append(&r,B.v[bi].p,B.v[bi].n);bi++;}int ok=r.len==strlen(b)&&!memcmp(r.data,b,r.len);cvc_buf_free(&r);cvc_diffops_free(&op);cvc_lines_free(&A);cvc_lines_free(&B);return ok;}
int main(void){
 uint8_t d[32];
 cvc_sha256("",0,d);T("sha empty",hex_is(d,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
 cvc_sha256("abc",3,d);T("sha abc",hex_is(d,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
 const char*longv="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";cvc_sha256(longv,strlen(longv),d);T("sha multiblock",hex_is(d,"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
 CvcSha256 sh;cvc_sha256_init(&sh);for(size_t i=0;i<strlen(longv);i++)cvc_sha256_update(&sh,longv+i,1);cvc_sha256_final(&sh,d);T("sha byte chunks",hex_is(d,"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
 cvc_sha256_init(&sh);cvc_sha256_update(&sh,"a",1);cvc_sha256_update(&sh,"bc",2);cvc_sha256_final(&sh,d);T("sha split chunks",hex_is(d,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
 T("json minimal",js("{}")); T("json nested",js("{\"a\":[1,true,null,{\"x\":\"y\"}]}"));
 T("json escapes",js("{\"x\":\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"}"));T("json BMP",js("{\"x\":\"\\u4e2d\"}"));T("json surrogate",js("{\"x\":\"\\ud83d\\ude00\"}"));
 T("json high surrogate reject",!js("{\"x\":\"\\ud83d\"}"));T("json low surrogate reject",!js("{\"x\":\"\\ude00\"}"));
 {unsigned char q[]={ '{','\"','x','\"',':','\"',0xff,'\"','}'};T("json invalid UTF8",!jp(q,sizeof(q)));}
 T("json duplicate",!js("{\"a\":1,\"a\":2}"));T("json decoded duplicate",!js("{\"a\":1,\"\\u0061\":2}"));T("json trailing comma object",!js("{\"a\":1,}"));T("json trailing comma array",!js("[1,]"));T("json comments",!js("{/*x*/\"a\":1}"));T("json garbage",!js("{}x"));
 {unsigned char q[]={0xef,0xbb,0xbf,'{','}'};T("json BOM",!jp(q,sizeof(q)));}
 T("json number leading zero",!js("01"));T("json number incomplete frac",!js("1."));T("json number incomplete exp",!js("1e+"));T("json overflow",!js("1e99999"));T("json nul length aware",js("\"a\\u0000b\""));
 T("glob star no slash",cvc_glob_match("*.c","a.c")&&!cvc_glob_match("*.c","d/a.c"));T("glob question",cvc_glob_match("a?c","abc")&&!cvc_glob_match("a?c","a/c"));T("glob dstar",cvc_glob_match("a/**/z","a/x/y/z"));T("glob dstar slash zero",cvc_glob_match("**/*.md","x.md"));T("glob dstar root",cvc_glob_match("**","a/b/c"));T("glob triple reject",!cvc_glob_validate("a/***/b"));T("glob Chinese exact",cvc_glob_match("資料/檔案.txt","資料/檔案.txt"));
 T("myers identical",myers_reconstruct("a\nb\n","a\nb\n"));T("myers add front",myers_reconstruct("b\n","a\nb\n"));T("myers add end",myers_reconstruct("a\n","a\nb\n"));T("myers delete middle",myers_reconstruct("a\nb\nc\n","a\nc\n"));T("myers replace",myers_reconstruct("a\nb\n","a\nx\n"));T("myers repeats",myers_reconstruct("x\na\na\ny\n","x\na\ny\n"));T("myers no-final-newline",myers_reconstruct("a\n","a"));T("myers CRLF",myers_reconstruct("a\n","a\r\n"));T("myers unicode",myers_reconstruct("中文\n😀\n","中文\n🙂\n"));
 T("3way disjoint",three("a\nb\nc\n","A\nb\nc\n","a\nb\nC\n","A\nb\nC\n",0));T("3way identical edit",three("a\nb\n","a\nX\n","a\nX\n","a\nX\n",0));T("3way overlap conflict",three("a\nb\n","a\nX\n","a\nY\n","",1));T("3way same insert once",three("a\n","z\na\n","z\na\n","z\na\n",0));T("3way different insert conflict",three("a\n","x\na\n","y\na\n","",1));
 cvc_object_oid(CVC_OBJ_BLOB,"",0,d);T("canonical empty blob",hex_is(d,"473a0f4c3be8a93681a267e3b1e9a7dcda1185436fe141f7749120a303721813"));
 cvc_object_oid(CVC_OBJ_BLOB,"abc",3,d);T("canonical abc blob",hex_is(d,"c1cf6e465077930e88dc5136641d402f72a229ddd996f627d60e9639eaba35a6"));
 {unsigned char z[4]={0};cvc_object_oid(CVC_OBJ_TREE,z,4,d);T("canonical empty tree",hex_is(d,"37b344f390f440a6a43040c9b0da9937d8f0d9d2b4db80cd1e2385054835c50f"));}
 {uint8_t root[32];cvc_hex_decode_32("37b344f390f440a6a43040c9b0da9937d8f0d9d2b4db80cd1e2385054835c50f",root);CvcBuf p;cvc_buf_init(&p);cvc_buf_append(&p,root,32);cvc_buf_putc(&p,0);cvc_put_u64be(&p,0);cvc_put_u64be(&p,1);cvc_buf_putc(&p,'x');cvc_object_oid(CVC_OBJ_COMMIT,p.data,p.len,d);cvc_buf_free(&p);T("canonical root commit",hex_is(d,"b76903cf9661046c99f6f4d4e9ceda05cef2607b47bd9b2f9396ea67ad1e72ab"));}
 printf("UNIT SUMMARY: %d/%d passed\n",total-failed,total);return failed?1:0;
}
