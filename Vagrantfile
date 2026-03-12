Vagrant.configure("2") do |config|
  config.vm.box = "ubuntu/focal64"
  config.vm.provider "virtualbox" do |vb|
    vb.memory = "2048"
    vb.cpus = 2
  end
  config.vm.provision "shell", inline: <<-SHELL
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y build-essential cmake git qt6-base-dev qt6-tools-dev libqt6svg6-dev libqt6core5compat6-dev qtkeychain-qt6-dev libqt6keychain1 libkf6xmlgui-dev kf6-kxmlgui-dev xorg-server-xvfb imagemagick xdotool xvfb x11-utils

    # KMagMuxMock build
    cd /vagrant
    cmake -B build -S .
    cmake --build build -j$(nproc)
  SHELL
end
