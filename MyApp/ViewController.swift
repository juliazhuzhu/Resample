//
//  ViewController.swift
//  MyApp
//
//  Created by 晓叶 on 2023/3/3.
//

import Cocoa

class ViewController: NSViewController {
    
    var recordStatus :Bool = false
    let bt = NSButton.init(title: "开始录制", target: self, action: #selector(myFunc))
    var thread :Thread?
    override func viewDidLoad() {
        super.viewDidLoad()

        // Do any additional setup after loading the view.
        self.view.setFrameSize(NSSize(width: 320, height: 240))
        
        bt.title = "开始录制"
        bt.frame = NSRect(x: 320/2-60, y: 240/2-15, width:120, height: 60)
        bt.bezelStyle = .rounded
        bt.setButtonType(.pushOnPushOff)
        self.view.addSubview(bt)
    }
    
    @objc
    func myFunc() {
        recordStatus = !recordStatus
        if recordStatus == true {
            self.bt.title = "停止录制"
            thread = Thread.init(target: self, selector: #selector(self.recAudio), object: nil)
            thread?.start()
        }else {
            self.bt.title = "开始录制"
            stop_record();
           
          
        }
    }
    
    @objc
    func recAudio() {
        record_audio();
    }
    override var representedObject: Any? {
        didSet {
        // Update the view, if already loaded.
        }
    }


}

