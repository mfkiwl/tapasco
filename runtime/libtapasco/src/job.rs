use crate::device::DataTransferPrealloc;
use crate::device::PEParameter;
use crate::pe::PE;
use crate::scheduler::Scheduler;
use snafu::ResultExt;
use std::sync::Arc;

impl<T> From<std::sync::PoisonError<T>> for Error {
    fn from(_error: std::sync::PoisonError<T>) -> Self {
        Error::MutexError {}
    }
}

#[derive(Debug, Snafu)]
pub enum Error {
    #[snafu(display("Allocator Error: {}", source))]
    AllocatorError { source: crate::allocator::Error },

    #[snafu(display("DMA Error: {}", source))]
    DMAError { source: crate::dma::Error },

    #[snafu(display("Mutex has been poisoned"))]
    MutexError {},

    #[snafu(display("PE Error: {}", source))]
    PEError { source: crate::pe::Error },

    #[snafu(display(
        "Unsupported parameter during register write stage. Unconverted data transfer alloc?: {:?}",
        arg
    ))]
    UnsupportedRegisterParameter { arg: PEParameter },

    #[snafu(display(
        "Unsupported parameter during during transfer to. Unconverted data transfer alloc?: {:?}",
        arg
    ))]
    UnsupportedTransferParameter { arg: PEParameter },

    #[snafu(display("Scheduler Error: {}", source))]
    SchedulerError { source: crate::scheduler::Error },
}

type Result<T, E = Error> = std::result::Result<T, E>;

#[derive(Debug)]
pub struct Job {
    pe: Option<PE>,
    scheduler: Arc<Scheduler>,
}

impl Drop for Job {
    fn drop(&mut self) {
        match self.release(true) {
            Ok(_) => (),
            Err(e) => panic!("{}", e),
        }
    }
}

impl Job {
    pub fn new(pe: PE, scheduler: &Arc<Scheduler>) -> Job {
        Job {
            pe: Some(pe),
            scheduler: scheduler.clone(),
        }
    }

    //TODO: Check performance as this does not happen inplace but creates a new Vec
    pub fn handle_allocates(&self, args: Vec<PEParameter>) -> Result<Vec<PEParameter>> {
        trace!("Handling allocate parameters.");
        let new_params = args
            .into_iter()
            .map(|arg| match arg {
                PEParameter::DataTransferAlloc(x) => {
                    let a = {
                        x.memory
                            .allocator()
                            .lock()?
                            .allocate(x.data.len() as u64)
                            .context(AllocatorError)?
                    };
                    Ok(PEParameter::DataTransferPrealloc(DataTransferPrealloc {
                        data: x.data,
                        device_address: a,
                        from_device: x.from_device,
                        to_device: x.to_device,
                        memory: x.memory,
                        free: x.free,
                    }))
                }
                _ => Ok(arg),
            })
            .collect();
        trace!("All allocate parameters handled.");
        new_params
    }

    pub fn handle_transfers_to_device(
        &mut self,
        args: Vec<PEParameter>,
    ) -> Result<Vec<PEParameter>> {
        trace!("Handling allocate parameters.");
        let new_params = args
            .into_iter()
            .try_fold(Vec::new(), |mut xs, arg| match arg {
                PEParameter::DataTransferPrealloc(x) => {
                    if x.to_device {
                        x.memory
                            .dma()
                            .copy_to(&x.data[..], x.device_address)
                            .context(DMAError)?;
                        xs.push(PEParameter::DeviceAddress(x.device_address));
                    }
                    if x.from_device {
                        self.pe.as_mut().unwrap().add_copyback(x);
                    }
                    Ok(xs)
                }
                _ => {
                    xs.push(arg);
                    Ok(xs)
                }
            });
        trace!("All transfer to parameters handled.");
        new_params
    }

    pub fn start(&mut self, args: Vec<PEParameter>) -> Result<()> {
        trace!(
            "Starting execution of {:?} with Arguments {:?}.",
            self.pe,
            args
        );
        let local_args = self.handle_allocates(args)?;
        trace!("Handled allocates => {:?}.", local_args);
        let trans_args = self.handle_transfers_to_device(local_args)?;
        trace!("Handled transfers => {:?}.", trans_args);
        trace!("Setting arguments.");
        for (i, arg) in trans_args.into_iter().enumerate() {
            trace!("Setting argument {} => {:?}.", i, arg);
            match arg {
                PEParameter::Single32(_) => {
                    self.pe.as_ref().unwrap().set_arg(i, arg).context(PEError)?
                }
                PEParameter::Single64(_) => {
                    self.pe.as_ref().unwrap().set_arg(i, arg).context(PEError)?
                }
                PEParameter::DeviceAddress(x) => self
                    .pe
                    .as_ref()
                    .unwrap()
                    .set_arg(i, PEParameter::Single64(x))
                    .context(PEError)?,
                _ => return Err(Error::UnsupportedRegisterParameter { arg: arg }),
            };
        }
        trace!("Arguments set.");
        trace!("Starting PE {} execution.", self.pe.as_ref().unwrap().id());
        self.pe.as_mut().unwrap().start().context(PEError)?;
        trace!("PE {} started.", self.pe.as_ref().unwrap().id());
        Ok(())
    }

    pub fn release(&mut self, release_pe: bool) -> Result<Option<Vec<Vec<u8>>>> {
        if self.pe.is_some() {
            trace!("Trying to release PE {:?}.", self.pe.as_ref().unwrap().id());
            let copyback = self.pe.as_mut().unwrap().release().context(PEError)?;
            trace!("PE is idle.");

            if release_pe {
                self.scheduler
                    .release_pe(self.pe.take().unwrap())
                    .context(SchedulerError)?;
            }
            trace!("Release successful.");
            match copyback {
                Some(x) => {
                    let res = x
                        .into_iter()
                        .map(|mut param| {
                            param
                                .memory
                                .dma()
                                .copy_from(param.device_address, &mut param.data[..])
                                .context(DMAError)?;
                            if param.free {
                                param
                                    .memory
                                    .allocator()
                                    .lock()?
                                    .free(param.device_address)
                                    .context(AllocatorError)?;
                            }
                            Ok(param.data)
                        })
                        .collect();
                    match res {
                        Ok(x) => Ok(Some(x)),
                        Err(x) => Err(x),
                    }
                }
                None => Ok(None),
            }
        } else {
            Ok(None)
        }
    }
}