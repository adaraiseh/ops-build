inherit core-image extrausers
EXTRA_USERS_PARAMS = "\
         useradd -P admin admin; \
         "
IMAGE_FEATURES += "ssh-server-openssh"