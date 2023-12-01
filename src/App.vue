<script setup lang="ts">
import {ref} from 'vue'
import {ElMessage} from "element-plus";
import Loading from "./components/Loading.vue";
import Form from "./components/Form.vue";
import Button1 from "./components/Button1.vue";
import Button2 from "./components/Button2.vue";
import Button3 from "./components/Button3.vue";
import Button4 from "./components/Button4.vue";
import Button5 from "./components/Button5.vue";

const showLoading = ref(false)
const dialogFormVisible = ref(true)
const password = ref('')
const msg = ref('')

const loadModule = () => {
  if (isModuleLoaded()) {
    ElMessage.warning("模块已加载！")
    return
  }
  showLoading.value = true
  setTimeout(() => {
    window.ipcRenderer.sendSync('cli', 'echo ' + password.value + '|sudo -S insmod mymodule.ko')
    showLoading.value = false
    ElMessage.success("模块加载完毕！")
  }, 500)
}

const unloadModule = () => {
  if (!isModuleLoaded()) {
    ElMessage.warning("模块未加载！")
    return
  }
  showLoading.value = true
  setTimeout(() => {
    window.ipcRenderer.sendSync('cli', 'echo ' + password.value + '|sudo -S rmmod mymodule.ko')
    showLoading.value = false
    ElMessage.success("模块卸载完毕！")
  }, 500)
}
const isModuleLoaded = () => {
  return !!window.ipcRenderer.sendSync('cli', 'cat /proc/devices|grep char_dev').stdout;
}

const clearMsg = () => {
  showLoading.value = true
  setTimeout(()=>{
    window.ipcRenderer.sendSync('cli', 'echo ' + password.value + '|sudo -S dmesg --clear')
    showLoading.value = false
    ElMessage.success("内核环形缓冲区清理完毕！")
  },500)
}

const catMsg = () => {
  showLoading.value = true
  setTimeout(()=>{
    msg.value= window.ipcRenderer.sendSync('cli', 'echo ' + password.value + '|sudo -S dmesg|grep mymodule').stdout;
    showLoading.value = false
    ElMessage.success("内核环形缓冲区信息加载完毕！")
  },500)
}

const testModule = () => {
  showLoading.value = true
  setTimeout(()=>{
    window.ipcRenderer.sendSync('cli', 'echo ' + password.value + '|sudo -S ./test_module 测试')
    showLoading.value = false
    ElMessage.success("测试完毕，向内核模块发送信息：‘测试’")
  },500)
}
const dialogConfirmHandler = () => {
  showLoading.value = true
  setTimeout(() => {
    if (!window.ipcRenderer.sendSync('cli', 'echo ' + password.value + '|sudo -S ls').stdout) {
      showLoading.value = false
      ElMessage.error("密码错误，请重新输入！")
      return
    }
    showLoading.value = false
    dialogFormVisible.value = false
    clearMsg()
    loadModule()
  }, 50)
}
</script>

<template>
  <Form :show="dialogFormVisible" v-model:passwd="password" @submit="dialogConfirmHandler"/>
  <div class="main" v-if="!dialogFormVisible">
    <div class="left">
      <div class="button-container"><button1 @click="loadModule" text="加载模块"/></div>
      <div class="button-container"><button2 @click="unloadModule" text="卸载模块"/></div>
      <div class="button-container"><button3 @click="testModule" text="测试模块"/></div>
      <div class="button-container"><button4 @click="clearMsg" text="清空内核环形缓冲区"/></div>
      <div class="button-container"><button5 @click="catMsg" text="显示内核环形缓冲区"/></div>
    </div>
    <div class="right">
      <pre style="text-align: left">{{msg}}</pre>
    </div>
  </div>
  <Loading v-show="showLoading"/>

</template>

<style scoped>
.left {
  position: absolute;
  top: 0;
  left: 0;
  display: flex;
  flex-direction: column;
  justify-content: space-around;
  align-items: center;
  padding: 10px;
  overflow: hidden;
  height: 100vh;
  width: 200px;
  text-align: left;
}
.right{
  position: absolute;
  top: 20%;
  left: 220px;
  width: 550px;
  height: 60vh;
  overflow-y: auto;
  overflow-x: hidden;
}
</style>