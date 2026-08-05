import os
from conan import ConanFile
from conan.tools.meson import Meson, MesonToolchain
from conan.tools.gnu import PkgConfigDeps

class SKYMRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"

    def layout(self):        
        arch = os.environ.get("VSCMD_ARG_TGT_ARCH", str(self.settings.arch))
        self.folders.build = f"builddir-{arch}"
        self.folders.generators = f"generators-{arch}"
        self.folders.source = "."        
        
    def requirements(self):
        self.requires("botan/3.10.0", options={
            "shared": False, 
            "enable_modules":"ecdsa,ed25519,ed448,rsa,system_rng,dl_algo,auto_rng,pcurves_secp256r1,pcurves_secp384r1,pcurves_secp521r1,argon2,argon2fmt,argon2_avx2,ec_group,legacy_ec_point,pbes2,aes,aes_ni,sha1,sha2_64,emsa_pkcs1"
            })
        self.requires("wtl/10.0.9163")

    def generate(self):
        tc = MesonToolchain(self)
        tc.pkg_config_path = self.generators_folder       
        tc.generate()
        pc = PkgConfigDeps(self)
        pc.generate()
    
    def build(self):        
        meson = Meson(self)        
        meson.configure()
        meson.build()
   
